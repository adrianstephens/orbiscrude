#include "unreal.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "comms/zlib_stream.h"
#include "utilities.h"
#include "3d/model_utils.h"
#include "bitmap/bitmap.h"
#include "codec/texels/dxt.h"
#include "codec/texels/att.h"
#include "codec/texels/astc.h"
#include "codec/texels/etc.h"
#include "codec/oodle.h"
#include "archive_help.h"

namespace unreal {

struct UnrealVertex {
	float3p	pos;
	float3p	norm;
};
ISO_DEFUSERCOMPV(UnrealVertex, pos, norm);

//namespace ISO {
//template<typename T> tag2	_GetName(Ptr<t> &p)		{ p.ID(); }
//}

enum EUnrealEngineObjectUE4Version {
	VER_UE4_OLDEST_LOADABLE_PACKAGE = 214,

	VER_UE4_BLUEPRINT_VARS_NOT_READ_ONLY,
	VER_UE4_STATIC_MESH_STORE_NAV_COLLISION,
	VER_UE4_ATMOSPHERIC_FOG_DECAY_NAME_CHANGE,
	VER_UE4_SCENECOMP_TRANSLATION_TO_LOCATION,
	VER_UE4_MATERIAL_ATTRIBUTES_REORDERING,
	VER_UE4_COLLISION_PROFILE_SETTING,
	VER_UE4_BLUEPRINT_SKEL_TEMPORARY_TRANSIENT,
	VER_UE4_BLUEPRINT_SKEL_SERIALIZED_AGAIN,
	VER_UE4_BLUEPRINT_SETS_REPLICATION,
	VER_UE4_WORLD_LEVEL_INFO,
	VER_UE4_AFTER_CAPSULE_HALF_HEIGHT_CHANGE,
	VER_UE4_ADDED_NAMESPACE_AND_KEY_DATA_TO_FTEXT,
	VER_UE4_ATTENUATION_SHAPES,
	VER_UE4_LIGHTCOMPONENT_USE_IES_TEXTURE_MULTIPLIER_ON_NON_IES_BRIGHTNESS,
	VER_UE4_REMOVE_INPUT_COMPONENTS_FROM_BLUEPRINTS,
	VER_UE4_VARK2NODE_USE_MEMBERREFSTRUCT,
	VER_UE4_REFACTOR_MATERIAL_EXPRESSION_SCENECOLOR_AND_SCENEDEPTH_INPUTS,
	VER_UE4_SPLINE_MESH_ORIENTATION,
	VER_UE4_REVERB_EFFECT_ASSET_TYPE,
	VER_UE4_MAX_TEXCOORD_INCREASED,
	VER_UE4_SPEEDTREE_STATICMESH,
	VER_UE4_LANDSCAPE_COMPONENT_LAZY_REFERENCES,
	VER_UE4_SWITCH_CALL_NODE_TO_USE_MEMBER_REFERENCE,
	VER_UE4_ADDED_SKELETON_ARCHIVER_REMOVAL,
	VER_UE4_ADDED_SKELETON_ARCHIVER_REMOVAL_SECOND_TIME,
	VER_UE4_BLUEPRINT_SKEL_CLASS_TRANSIENT_AGAIN,
	VER_UE4_ADD_COOKED_TO_UCLASS,
	VER_UE4_DEPRECATED_STATIC_MESH_THUMBNAIL_PROPERTIES_REMOVED,
	VER_UE4_COLLECTIONS_IN_SHADERMAPID,
	VER_UE4_REFACTOR_MOVEMENT_COMPONENT_HIERARCHY,
	VER_UE4_FIX_TERRAIN_LAYER_SWITCH_ORDER,
	VER_UE4_ALL_PROPS_TO_CONSTRAINTINSTANCE,
	VER_UE4_LOW_QUALITY_DIRECTIONAL_LIGHTMAPS,
	VER_UE4_ADDED_NOISE_EMITTER_COMPONENT,
	VER_UE4_ADD_TEXT_COMPONENT_VERTICAL_ALIGNMENT,
	VER_UE4_ADDED_FBX_ASSET_IMPORT_DATA,
	VER_UE4_REMOVE_LEVELBODYSETUP,
	VER_UE4_REFACTOR_CHARACTER_CROUCH,
	VER_UE4_SMALLER_DEBUG_MATERIALSHADER_UNIFORM_EXPRESSIONS,
	VER_UE4_APEX_CLOTH,
	VER_UE4_SAVE_COLLISIONRESPONSE_PER_CHANNEL,
	VER_UE4_ADDED_LANDSCAPE_SPLINE_EDITOR_MESH,
	VER_UE4_CHANGED_MATERIAL_REFACTION_TYPE,
	VER_UE4_REFACTOR_PROJECTILE_MOVEMENT,
	VER_UE4_REMOVE_PHYSICALMATERIALPROPERTY,
	VER_UE4_PURGED_FMATERIAL_COMPILE_OUTPUTS,
	VER_UE4_ADD_COOKED_TO_LANDSCAPE,
	VER_UE4_CONSUME_INPUT_PER_BIND,
	VER_UE4_SOUND_CLASS_GRAPH_EDITOR,
	VER_UE4_FIXUP_TERRAIN_LAYER_NODES,
	VER_UE4_RETROFIT_CLAMP_EXPRESSIONS_SWAP,
	VER_UE4_REMOVE_LIGHT_MOBILITY_CLASSES,
	VER_UE4_REFACTOR_PHYSICS_BLENDING,
	VER_UE4_WORLD_LEVEL_INFO_UPDATED,
	VER_UE4_STATIC_SKELETAL_MESH_SERIALIZATION_FIX,
	VER_UE4_REMOVE_STATICMESH_MOBILITY_CLASSES,
	VER_UE4_REFACTOR_PHYSICS_TRANSFORMS,
	VER_UE4_REMOVE_ZERO_TRIANGLE_SECTIONS,
	VER_UE4_CHARACTER_MOVEMENT_DECELERATION,
	VER_UE4_CAMERA_ACTOR_USING_CAMERA_COMPONENT,
	VER_UE4_CHARACTER_MOVEMENT_DEPRECATE_PITCH_ROLL,
	VER_UE4_REBUILD_TEXTURE_STREAMING_DATA_ON_LOAD,
	VER_UE4_SUPPORT_32BIT_STATIC_MESH_INDICES,
	VER_UE4_ADDED_CHUNKID_TO_ASSETDATA_AND_UPACKAGE,
	VER_UE4_CHARACTER_DEFAULT_MOVEMENT_BINDINGS,
	VER_UE4_APEX_CLOTH_LOD,
	VER_UE4_ATMOSPHERIC_FOG_CACHE_DATA,
	VAR_UE4_ARRAY_PROPERTY_INNER_TAGS,
	VER_UE4_KEEP_SKEL_MESH_INDEX_DATA,
	VER_UE4_BODYSETUP_COLLISION_CONVERSION,
	VER_UE4_REFLECTION_CAPTURE_COOKING,
	VER_UE4_REMOVE_DYNAMIC_VOLUME_CLASSES,
	VER_UE4_STORE_HASCOOKEDDATA_FOR_BODYSETUP,
	VER_UE4_REFRACTION_BIAS_TO_REFRACTION_DEPTH_BIAS,
	VER_UE4_REMOVE_SKELETALPHYSICSACTOR,
	VER_UE4_PC_ROTATION_INPUT_REFACTOR,
	VER_UE4_LANDSCAPE_PLATFORMDATA_COOKING,
	VER_UE4_CREATEEXPORTS_CLASS_LINKING_FOR_BLUEPRINTS,
	VER_UE4_REMOVE_NATIVE_COMPONENTS_FROM_BLUEPRINT_SCS,
	VER_UE4_REMOVE_SINGLENODEINSTANCE,
	VER_UE4_CHARACTER_BRAKING_REFACTOR,
	VER_UE4_VOLUME_SAMPLE_LOW_QUALITY_SUPPORT,
	VER_UE4_SPLIT_TOUCH_AND_CLICK_ENABLES,
	VER_UE4_HEALTH_DEATH_REFACTOR,
	VER_UE4_SOUND_NODE_ENVELOPER_CURVE_CHANGE,
	VER_UE4_POINT_LIGHT_SOURCE_RADIUS,
	VER_UE4_SCENE_CAPTURE_CAMERA_CHANGE,
	VER_UE4_MOVE_SKELETALMESH_SHADOWCASTING,
	VER_UE4_CHANGE_SETARRAY_BYTECODE,
	VER_UE4_MATERIAL_INSTANCE_BASE_PROPERTY_OVERRIDES,
	VER_UE4_COMBINED_LIGHTMAP_TEXTURES,
	VER_UE4_BUMPED_MATERIAL_EXPORT_GUIDS,
	VER_UE4_BLUEPRINT_INPUT_BINDING_OVERRIDES,
	VER_UE4_FIXUP_BODYSETUP_INVALID_CONVEX_TRANSFORM,
	VER_UE4_FIXUP_STIFFNESS_AND_DAMPING_SCALE,
	VER_UE4_REFERENCE_SKELETON_REFACTOR,
	VER_UE4_K2NODE_REFERENCEGUIDS,
	VER_UE4_FIXUP_ROOTBONE_PARENT,
	VER_UE4_TEXT_RENDER_COMPONENTS_WORLD_SPACE_SIZING,
	VER_UE4_MATERIAL_INSTANCE_BASE_PROPERTY_OVERRIDES_PHASE_2,
	VER_UE4_CLASS_NOTPLACEABLE_ADDED,
	VER_UE4_WORLD_LEVEL_INFO_LOD_LIST,
	VER_UE4_CHARACTER_MOVEMENT_VARIABLE_RENAMING_1,
	VER_UE4_FSLATESOUND_CONVERSION,
	VER_UE4_WORLD_LEVEL_INFO_ZORDER,
	VER_UE4_PACKAGE_REQUIRES_LOCALIZATION_GATHER_FLAGGING,
	VER_UE4_BP_ACTOR_VARIABLE_DEFAULT_PREVENTING,
	VER_UE4_TEST_ANIMCOMP_CHANGE,
	VER_UE4_EDITORONLY_BLUEPRINTS,
	VER_UE4_EDGRAPHPINTYPE_SERIALIZATION,
	VER_UE4_NO_MIRROR_BRUSH_MODEL_COLLISION,
	VER_UE4_CHANGED_CHUNKID_TO_BE_AN_ARRAY_OF_CHUNKIDS,
	VER_UE4_WORLD_NAMED_AFTER_PACKAGE,
	VER_UE4_SKY_LIGHT_COMPONENT,
	VER_UE4_WORLD_LAYER_ENABLE_DISTANCE_STREAMING,
	VER_UE4_REMOVE_ZONES_FROM_MODEL,
	VER_UE4_FIX_ANIMATIONBASEPOSE_SERIALIZATION,
	VER_UE4_SUPPORT_8_BONE_INFLUENCES_SKELETAL_MESHES,
	VER_UE4_ADD_OVERRIDE_GRAVITY_FLAG,
	VER_UE4_SUPPORT_GPUSKINNING_8_BONE_INFLUENCES,
	VER_UE4_ANIM_SUPPORT_NONUNIFORM_SCALE_ANIMATION,
	VER_UE4_ENGINE_VERSION_OBJECT,
	VER_UE4_PUBLIC_WORLDS,
	VER_UE4_SKELETON_GUID_SERIALIZATION,
	VER_UE4_CHARACTER_MOVEMENT_WALKABLE_FLOOR_REFACTOR,
	VER_UE4_INVERSE_SQUARED_LIGHTS_DEFAULT,
	VER_UE4_DISABLED_SCRIPT_LIMIT_BYTECODE,
	VER_UE4_PRIVATE_REMOTE_ROLE,
	VER_UE4_FOLIAGE_STATIC_MOBILITY,
	VER_UE4_BUILD_SCALE_VECTOR,
	VER_UE4_FOLIAGE_COLLISION,
	VER_UE4_SKY_BENT_NORMAL,
	VER_UE4_LANDSCAPE_COLLISION_DATA_COOKING,
	VER_UE4_MORPHTARGET_CPU_TANGENTZDELTA_FORMATCHANGE,
	VER_UE4_SOFT_CONSTRAINTS_USE_MASS,
	VER_UE4_REFLECTION_DATA_IN_PACKAGES,
	VER_UE4_FOLIAGE_MOVABLE_MOBILITY,
	VER_UE4_UNDO_BREAK_MATERIALATTRIBUTES_CHANGE,
	VER_UE4_ADD_CUSTOMPROFILENAME_CHANGE,
	VER_UE4_FLIP_MATERIAL_COORDS,
	VER_UE4_MEMBERREFERENCE_IN_PINTYPE,
	VER_UE4_VEHICLES_UNIT_CHANGE,
	VER_UE4_ANIMATION_REMOVE_NANS,
	VER_UE4_SKELETON_ASSET_PROPERTY_TYPE_CHANGE,
	VER_UE4_FIX_BLUEPRINT_VARIABLE_FLAGS,
	VER_UE4_VEHICLES_UNIT_CHANGE2,
	VER_UE4_UCLASS_SERIALIZE_INTERFACES_AFTER_LINKING,
	VER_UE4_STATIC_MESH_SCREEN_SIZE_LODS,
	VER_UE4_FIX_MATERIAL_COORDS,
	VER_UE4_SPEEDTREE_WIND_V7,
	VER_UE4_LOAD_FOR_EDITOR_GAME,
	VER_UE4_SERIALIZE_RICH_CURVE_KEY,
	VER_UE4_MOVE_LANDSCAPE_MICS_AND_TEXTURES_WITHIN_LEVEL,
	VER_UE4_FTEXT_HISTORY,
	VER_UE4_FIX_MATERIAL_COMMENTS,
	VER_UE4_STORE_BONE_EXPORT_NAMES,
	VER_UE4_MESH_EMITTER_INITIAL_ORIENTATION_DISTRIBUTION,
	VER_UE4_DISALLOW_FOLIAGE_ON_BLUEPRINTS,
	VER_UE4_FIXUP_MOTOR_UNITS,
	VER_UE4_DEPRECATED_MOVEMENTCOMPONENT_MODIFIED_SPEEDS,
	VER_UE4_RENAME_CANBECHARACTERBASE,
	VER_UE4_GAMEPLAY_TAG_CONTAINER_TAG_TYPE_CHANGE,
	VER_UE4_FOLIAGE_SETTINGS_TYPE,
	VER_UE4_STATIC_SHADOW_DEPTH_MAPS,
	VER_UE4_ADD_TRANSACTIONAL_TO_DATA_ASSETS,
	VER_UE4_ADD_LB_WEIGHTBLEND,
	VER_UE4_ADD_ROOTCOMPONENT_TO_FOLIAGEACTOR,
	VER_UE4_FIX_MATERIAL_PROPERTY_OVERRIDE_SERIALIZE,
	VER_UE4_ADD_LINEAR_COLOR_SAMPLER,
	VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP,
	VER_UE4_BLUEPRINT_USE_SCS_ROOTCOMPONENT_SCALE,
	VER_UE4_LEVEL_STREAMING_DRAW_COLOR_TYPE_CHANGE,
	VER_UE4_CLEAR_NOTIFY_TRIGGERS,
	VER_UE4_SKELETON_ADD_SMARTNAMES,
	VER_UE4_ADDED_CURRENCY_CODE_TO_FTEXT,
	VER_UE4_ENUM_CLASS_SUPPORT,
	VER_UE4_FIXUP_WIDGET_ANIMATION_CLASS,
	VER_UE4_SOUND_COMPRESSION_TYPE_ADDED,
	VER_UE4_AUTO_WELDING,
	VER_UE4_RENAME_CROUCHMOVESCHARACTERDOWN,
	VER_UE4_LIGHTMAP_MESH_BUILD_SETTINGS,
	VER_UE4_RENAME_SM3_TO_ES3_1,
	VER_UE4_DEPRECATE_UMG_STYLE_ASSETS,
	VER_UE4_POST_DUPLICATE_NODE_GUID,
	VER_UE4_RENAME_CAMERA_COMPONENT_VIEW_ROTATION,
	VER_UE4_CASE_PRESERVING_FNAME,
	VER_UE4_RENAME_CAMERA_COMPONENT_CONTROL_ROTATION,
	VER_UE4_FIX_REFRACTION_INPUT_MASKING,
	VER_UE4_GLOBAL_EMITTER_SPAWN_RATE_SCALE,
	VER_UE4_CLEAN_DESTRUCTIBLE_SETTINGS,
	VER_UE4_CHARACTER_MOVEMENT_UPPER_IMPACT_BEHAVIOR,
	VER_UE4_BP_MATH_VECTOR_EQUALITY_USES_EPSILON,
	VER_UE4_FOLIAGE_STATIC_LIGHTING_SUPPORT,
	VER_UE4_SLATE_COMPOSITE_FONTS,
	VER_UE4_REMOVE_SAVEGAMESUMMARY,

	VER_UE4_REMOVE_SKELETALMESH_COMPONENT_BODYSETUP_SERIALIZATION,
	VER_UE4_SLATE_BULK_FONT_DATA,
	VER_UE4_ADD_PROJECTILE_FRICTION_BEHAVIOR,
	VER_UE4_MOVEMENTCOMPONENT_AXIS_SETTINGS,
	VER_UE4_GRAPH_INTERACTIVE_COMMENTBUBBLES,
	VER_UE4_LANDSCAPE_SERIALIZE_PHYSICS_MATERIALS,
	VER_UE4_RENAME_WIDGET_VISIBILITY,
	VER_UE4_ANIMATION_ADD_TRACKCURVES,
	VER_UE4_MONTAGE_BRANCHING_POINT_REMOVAL,
	VER_UE4_BLUEPRINT_ENFORCE_CONST_IN_FUNCTION_OVERRIDES,
	VER_UE4_ADD_PIVOT_TO_WIDGET_COMPONENT,
	VER_UE4_PAWN_AUTO_POSSESS_AI,
	VER_UE4_FTEXT_HISTORY_DATE_TIMEZONE,
	VER_UE4_SORT_ACTIVE_BONE_INDICES,
	VER_UE4_PERFRAME_MATERIAL_UNIFORM_EXPRESSIONS,
	VER_UE4_MIKKTSPACE_IS_DEFAULT,
	VER_UE4_LANDSCAPE_GRASS_COOKING,
	VER_UE4_FIX_SKEL_VERT_ORIENT_MESH_PARTICLES,
	VER_UE4_LANDSCAPE_STATIC_SECTION_OFFSET,
	VER_UE4_ADD_MODIFIERS_RUNTIME_GENERATION,
	VER_UE4_MATERIAL_MASKED_BLENDMODE_TIDY,
	VER_UE4_MERGED_ADD_MODIFIERS_RUNTIME_GENERATION_TO_4_7_DEPRECATED,
	VER_UE4_AFTER_MERGED_ADD_MODIFIERS_RUNTIME_GENERATION_TO_4_7_DEPRECATED,
	VER_UE4_MERGED_ADD_MODIFIERS_RUNTIME_GENERATION_TO_4_7,
	VER_UE4_AFTER_MERGING_ADD_MODIFIERS_RUNTIME_GENERATION_TO_4_7,
	VER_UE4_SERIALIZE_LANDSCAPE_GRASS_DATA,
	VER_UE4_OPTIONALLY_CLEAR_GPU_EMITTERS_ON_INIT,
	VER_UE4_SERIALIZE_LANDSCAPE_GRASS_DATA_MATERIAL_GUID,
	VER_UE4_BLUEPRINT_GENERATED_CLASS_COMPONENT_TEMPLATES_PUBLIC,
	VER_UE4_ACTOR_COMPONENT_CREATION_METHOD,
	VER_UE4_K2NODE_EVENT_MEMBER_REFERENCE,
	VER_UE4_STRUCT_GUID_IN_PROPERTY_TAG,
	VER_UE4_REMOVE_UNUSED_UPOLYS_FROM_UMODEL,
	VER_UE4_REBUILD_HIERARCHICAL_INSTANCE_TREES,
	VER_UE4_PACKAGE_SUMMARY_HAS_COMPATIBLE_ENGINE_VERSION,
	VER_UE4_TRACK_UCS_MODIFIED_PROPERTIES,
	VER_UE4_LANDSCAPE_SPLINE_CROSS_LEVEL_MESHES,
	VER_UE4_DEPRECATE_USER_WIDGET_DESIGN_SIZE,
	VER_UE4_ADD_EDITOR_VIEWS,
	VER_UE4_FOLIAGE_WITH_ASSET_OR_CLASS,
	VER_UE4_BODYINSTANCE_BINARY_SERIALIZATION,
	VER_UE4_SERIALIZE_BLUEPRINT_EVENTGRAPH_FASTCALLS_IN_UFUNCTION,
	VER_UE4_INTERPCURVE_SUPPORTS_LOOPING,
	VER_UE4_MATERIAL_INSTANCE_BASE_PROPERTY_OVERRIDES_DITHERED_LOD_TRANSITION,
	VER_UE4_SERIALIZE_LANDSCAPE_ES2_TEXTURES,
	VER_UE4_CONSTRAINT_INSTANCE_MOTOR_FLAGS,
	VER_UE4_SERIALIZE_PINTYPE_CONST,
	VER_UE4_LIBRARY_CATEGORIES_AS_FTEXT,
	VER_UE4_SKIP_DUPLICATE_EXPORTS_ON_SAVE_PACKAGE,
	VER_UE4_SERIALIZE_TEXT_IN_PACKAGES,
	VER_UE4_ADD_BLEND_MODE_TO_WIDGET_COMPONENT,
	VER_UE4_NEW_LIGHTMASS_PRIMITIVE_SETTING,
	VER_UE4_REPLACE_SPRING_NOZ_PROPERTY,
	VER_UE4_TIGHTLY_PACKED_ENUMS,
	VER_UE4_ASSET_IMPORT_DATA_AS_JSON,
	VER_UE4_TEXTURE_LEGACY_GAMMA,
	VER_UE4_ADDED_NATIVE_SERIALIZATION_FOR_IMMUTABLE_STRUCTURES,
	VER_UE4_DEPRECATE_UMG_STYLE_OVERRIDES,
	VER_UE4_STATIC_SHADOWMAP_PENUMBRA_SIZE,
	VER_UE4_NIAGARA_DATA_OBJECT_DEV_UI_FIX,
	VER_UE4_FIXED_DEFAULT_ORIENTATION_OF_WIDGET_COMPONENT,
	VER_UE4_REMOVED_MATERIAL_USED_WITH_UI_FLAG,
	VER_UE4_CHARACTER_MOVEMENT_ADD_BRAKING_FRICTION,
	VER_UE4_BSP_UNDO_FIX,
	VER_UE4_DYNAMIC_PARAMETER_DEFAULT_VALUE,
	VER_UE4_STATIC_MESH_EXTENDED_BOUNDS,
	VER_UE4_ADDED_NON_LINEAR_TRANSITION_BLENDS,
	VER_UE4_AO_MATERIAL_MASK,
	VER_UE4_NAVIGATION_AGENT_SELECTOR,
	VER_UE4_MESH_PARTICLE_COLLISIONS_CONSIDER_PARTICLE_SIZE,
	VER_UE4_BUILD_MESH_ADJ_BUFFER_FLAG_EXPOSED,
	VER_UE4_MAX_ANGULAR_VELOCITY_DEFAULT,
	VER_UE4_APEX_CLOTH_TESSELLATION,
	VER_UE4_DECAL_SIZE,
	VER_UE4_KEEP_ONLY_PACKAGE_NAMES_IN_STRING_ASSET_REFERENCES_MAP,
	VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT,
	VER_UE4_DIALOGUE_WAVE_NAMESPACE_AND_CONTEXT_CHANGES,
	VER_UE4_MAKE_ROT_RENAME_AND_REORDER,
	VER_UE4_K2NODE_VAR_REFERENCEGUIDS,
	VER_UE4_SOUND_CONCURRENCY_PACKAGE,
	VER_UE4_USERWIDGET_DEFAULT_FOCUSABLE_FALSE,
	VER_UE4_BLUEPRINT_CUSTOM_EVENT_CONST_INPUT,
	VER_UE4_USE_LOW_PASS_FILTER_FREQ,
	VER_UE4_NO_ANIM_BP_CLASS_IN_GAMEPLAY_CODE,
	VER_UE4_SCS_STORES_ALLNODES_ARRAY,
	VER_UE4_FBX_IMPORT_DATA_RANGE_ENCAPSULATION,
	VER_UE4_CAMERA_COMPONENT_ATTACH_TO_ROOT,
	VER_UE4_INSTANCED_STEREO_UNIFORM_UPDATE,
	VER_UE4_STREAMABLE_TEXTURE_MIN_MAX_DISTANCE,
	VER_UE4_INJECT_BLUEPRINT_STRUCT_PIN_CONVERSION_NODES,
	VER_UE4_INNER_ARRAY_TAG_INFO,
	VER_UE4_FIX_SLOT_NAME_DUPLICATION,
	VER_UE4_STREAMABLE_TEXTURE_AABB,
	VER_UE4_PROPERTY_GUID_IN_PROPERTY_TAG,
	VER_UE4_NAME_HASHES_SERIALIZED,
	VER_UE4_INSTANCED_STEREO_UNIFORM_REFACTOR,
	VER_UE4_COMPRESSED_SHADER_RESOURCES,
	VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS,
	VER_UE4_TemplateIndex_IN_COOKED_EXPORTS,
	VER_UE4_PROPERTY_TAG_SET_MAP_SUPPORT,
	VER_UE4_ADDED_SEARCHABLE_NAMES,
	VER_UE4_64BIT_EXPORTMAP_SERIALSIZES,
	VER_UE4_SKYLIGHT_MOBILE_IRRADIANCE_MAP,
	VER_UE4_ADDED_SWEEP_WHILE_WALKING_FLAG,
	VER_UE4_ADDED_SOFT_OBJECT_PATH,
	VER_UE4_POINTLIGHT_SOURCE_ORIENTATION,
	VER_UE4_ADDED_PACKAGE_SUMMARY_LOCALIZATION_ID,
	VER_UE4_FIX_WIDE_STRING_CRC,
	VER_UE4_ADDED_PACKAGE_OWNER,

	VER_UE4_SKINWEIGHT_PROFILE_DATA_LAYOUT_CHANGES,
	VER_UE4_NON_OUTER_PACKAGE_IMPORT,
	VER_UE4_ASSETREGISTRY_DEPENDENCYFLAGS,
	VER_UE4_CORRECT_LICENSEE_FLAG,

	VER_UE4_AUTOMATIC_VERSION_PLUS_ONE,
	VER_UE4_AUTOMATIC_VERSION = VER_UE4_AUTOMATIC_VERSION_PLUS_ONE - 1,

	VER_UE5_INITIAL_VERSION = 1000,
	VER_UE5_NAMES_REFERENCED_FROM_EXPORT_DATA,
	VER_UE5_PAYLOAD_TOC,
	VER_UE5_OPTIONAL_RESOURCES,
	VER_UE5_LARGE_WORLD_COORDINATES,       
	VER_UE5_REMOVE_OBJECT_EXPORT_PACKAGE_GUID,
	VER_UE5_TRACK_OBJECT_EXPORT_IS_INHERITED,

	VER_UE5_AUTOMATIC_VERSION_PLUS_ONE,
	VER_UE5_AUTOMATIC_VERSION = VER_UE5_AUTOMATIC_VERSION_PLUS_ONE - 1
};

struct FEditorObjectVersion {
	enum Type {
		BeforeCustomVersionWasAdded = 0,
		GatheredTextProcessVersionFlagging,
		GatheredTextPackageCacheFixesV1,
		RootMetaDataSupport,
		GatheredTextPackageCacheFixesV2,
		TextFormatArgumentDataIsVariant,
		SplineComponentCurvesInStruct,
		ComboBoxControllerSupportUpdate,
		RefactorMeshEditorMaterials,
		AddedFontFaceAssets,
		UPropertryForMeshSection,
		WidgetGraphSchema,
		AddedBackgroundBlurContentSlot,
		StableUserDefinedEnumDisplayNames,
		AddedInlineFontFaceAssets,
		UPropertryForMeshSectionSerialize,
		FastWidgetTemplates,
		MaterialThumbnailRenderingChanges,
		NewSlateClippingSystem,
		MovieSceneMetaDataSerialization,
		GatheredTextEditorOnlyPackageLocId,
		AddedAlwaysSignNumberFormattingOption,
		AddedMaterialSharedInputs,
		AddedMorphTargetSectionIndices,
		SerializeInstancedStaticMeshRenderData,
		MeshDescriptionNewSerialization_MovedToRelease,
		MeshDescriptionNewAttributeFormat,
		ChangeSceneCaptureRootComponent,
		StaticMeshDeprecatedRawMesh,
		MeshDescriptionBulkDataGuid,
		MeshDescriptionRemovedHoles,
		ChangedWidgetComponentWindowVisibilityDefault,
		CultureInvariantTextSerializationKeyStability,
		ScrollBarThicknessChange,
		RemoveLandscapeHoleMaterial,
		MeshDescriptionTriangles,
		ComputeWeightedNormals,
		SkeletalMeshBuildRefactor,
		SkeletalMeshMoveEditorSourceDataToPrivateAsset,
		NumberParsingOptionsNumberLimitsAndClamping,
		SkeletalMeshSourceDataSupport16bitOfMaterialNumber,
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
	const static FGuid GUID;
};
const FGuid FEditorObjectVersion::GUID(0xE4B068ED, 0xF49442E9, 0xA231DA0B, 0x2E46BB41);

struct FUE5MainStreamObjectVersion {
	enum Type {
		BeforeCustomVersionWasAdded = 0,
		GeometryCollectionNaniteData,
		GeometryCollectionNaniteDDC,
		RemovingSourceAnimationData,
		MeshDescriptionNewFormat,
		PartitionActorDescSerializeGridGuid,
		ExternalActorsMapDataPackageFlag,
		AnimationAddedBlendProfileModes,
		WorldPartitionActorDescSerializeDataLayers,
		RenamingAnimationNumFrames,
		WorldPartitionHLODActorDescSerializeHLODLayer,
		GeometryCollectionNaniteCooked,
		AddedCookedBoolFontFaceAssets,
		WorldPartitionHLODActorDescSerializeCellHash,
		GeometryCollectionNaniteTransient,
		AddedLandscapeSplineActorDesc,
		AddCollisionConstraintFlag,
		MantleDbSerialize,
		AnimSyncGroupsExplicitSyncMethod,
		FLandscapeActorDescFixupGridIndices,
		FoliageTypeIncludeInHLOD,
		IntroducingAnimationDataModel,
		WorldPartitionActorDescSerializeActorLabel,
		WorldPartitionActorDescSerializeArchivePersistent,
		FixForceExternalActorLevelReferenceDuplicates,
		SerializeMeshDescriptionBase,
		ConvexUsesVerticesArray,
		WorldPartitionActorDescSerializeHLODInfo,
		AddDisabledFlag,
		MoveCustomAttributesToDataModel,
		BlendSpaceRuntimeTriangulation,
		BlendSpaceSmoothingImprovements,
		RemovingTessellationParameters,
		SparseClassDataStructSerialization,
		PackedLevelInstanceBoundsFix,
		AnimNodeConstantDataRefactorPhase0,
		MaterialSavedCachedData,
		RemoveDecalBlendMode,
		DirLightsAreAtmosphereLightsByDefault,
		WorldPartitionStreamingCellsNamingShortened,
		WorldPartitionActorDescGetStreamingBounds,
		MeshDescriptionVirtualization,
		TextureSourceVirtualization,
		RigVMCopyOpStoreNumBytes,
		MaterialTranslucencyPass,
		GeometryCollectionUserDefinedCollisionShapes,
		RemovedAtmosphericFog,
		SkyAtmosphereAffectsHeightFogWithBetterDefault,
		BlendSpaceSampleOrdering,
		GeometryCollectionCacheRemovesMassToLocal,
		EdGraphPinSourceIndex,
		VirtualizedBulkDataHaveUniqueGuids,
		RigVMMemoryStorageObject,
		RayTracedShadowsType,
		SkelMeshSectionVisibleInRayTracingFlagAdded,
		AnimGraphNodeTaggingAdded,
		DynamicMeshCompactedSerialization,
		ConvertReductionBaseSkeletalMeshBulkDataToInlineReductionCacheData,
		SkeletalMeshLODModelMeshInfo,
		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};
	const static FGuid GUID;
};
const FGuid FUE5MainStreamObjectVersion::GUID(0x697DD581, 0xE64f41AB, 0xAA4A51EC, 0xBEB7B628);


struct FStripDataFlags {
	enum EStrippedData {
		None	= 0,
		Editor	= 1,
		Server	= 2,
		All		= 0xff
	};

	uint8 GlobalStripFlags;
	uint8 ClassStripFlags;

	bool IsEditorDataStripped()				const	{ return (GlobalStripFlags & FStripDataFlags::Editor) != 0; }
	bool IsDataStrippedForServer()			const	{ return (GlobalStripFlags & FStripDataFlags::Server) != 0; }
	bool IsClassDataStripped(uint8 InFlags)	const	{ return (ClassStripFlags & InFlags) != 0; }
};

struct FCompressedChunkInfo {
	int64	CompressedSize;
	int64	UncompressedSize;
};

struct FCompressedChunk {
	int32	UncompressedOffset	= 0;
	int32	UncompressedSize	= 0;
	int32	CompressedOffset	= 0;
	int32	CompressedSize		= 0;
};

struct FNameEntrySerialized : FNameEntry {
	bool read(istream_ref file, bool hashes = true) {
		int32 len0 = file.get();
		if (bIsWide = len0 < 0)
			readn(file, WideName, Len = -len0);
		else
			readn(file, AnsiName, Len = len0);

		if (hashes) {
			uint16 DummyHashes[2];
			file.read(DummyHashes);
		}
		return true;
	}
	operator FString() const {
		if (bIsWide)
			return WideName;
		return AnsiName;
	}
};

struct FCustomVersionContainer {
	enum Type {
		Unknown,
		Guids,
		Enums,
		Optimized,
		CustomVersion_Automatic_Plus_One,
		Latest = CustomVersion_Automatic_Plus_One - 1
	};

	struct FCustomVersion {
		FGuid	Key;
		int32	Version;
		FString FriendlyName;
		FCustomVersion() {}
		FCustomVersion(FGuid InKey, int32 InVersion, FString&& FriendlyName) : Key(InKey), Version(InVersion), FriendlyName(move(FriendlyName)) {}
		bool read(istream_ref file)		{ return file.read(Key, Version); }
	};

	struct FEnumCustomVersion_DEPRECATED {
		uint32	Tag;
		int32	Version;
		operator FCustomVersion() const	{ return FCustomVersion(FGuid(0, 0, 0, Tag), Version, format_string("EnumTag%u", Tag).begin()); }
		bool read(istream_ref file)		{ return file.read(Tag, Version); }
	};

	struct FGuidCustomVersion_DEPRECATED {
		FGuid	Key;
		int32	Version;
		FString FriendlyName;
		operator FCustomVersion() const	{ return FCustomVersion(Key, Version, FString(FriendlyName)); }
		bool read(istream_ref file)		{ return file.read(Key, Version, FriendlyName); }
	};

	TArray<FCustomVersion> Versions;

	const FCustomVersion *Get(const FGuid &Key) const {
		for (auto& i : Versions) {
			if (i.Key == Key)
				return &i;
		}
		return nullptr;
	}

	bool read(istream_ref file, int32 LegacyFileVersion) {
		if (LegacyFileVersion < -5)
			Versions.read(file);
		else if (LegacyFileVersion < -2)
			Versions = TArray<FGuidCustomVersion_DEPRECATED>(file, file.get<int>());
		else if (LegacyFileVersion == -2)
			Versions = TArray<FEnumCustomVersion_DEPRECATED>(file, file.get<int>());
		else
			return false;
		return true;
	}

	bool read(istream_ref file, Type Format) {
		switch (Format) {
			case Enums:
				Versions = TArray<FEnumCustomVersion_DEPRECATED>(file, file.get<int>());
				return true;
			case Guids:
				Versions = TArray<FGuidCustomVersion_DEPRECATED>(file, file.get<int>());
				return true;
			case Optimized:
				return Versions.read(file);
			default:
				return false;
		}
	}
};

struct FEngineVersion {
	uint16	Major;
	uint16	Minor;
	uint16	Patch;
	uint32	Changelist;
	FString Branch;
	bool read(istream_ref file) {
		return file.read(Major, Minor,Patch, Changelist, Branch);
	}
};

struct FPackageFileSummary {
	enum : uint32 { PACKAGE_FILE_TAG = 0x9E2A83C1 };

	enum EUnrealEngineObjectLicenseeUE4Version {
		VER_LIC_NONE = 0,
		VER_LIC_AUTOMATIC_VERSION_PLUS_ONE,
		VER_LIC_AUTOMATIC_VERSION = VER_LIC_AUTOMATIC_VERSION_PLUS_ONE - 1
	};

	enum EPackageFlags {
		PKG_None						= 0x00000000,	// No flags
		PKG_NewlyCreated				= 0x00000001,	// Newly created package, not saved yet. In editor only
		PKG_ClientOptional				= 0x00000002,	// Purely optional for clients
		PKG_ServerSideOnly				= 0x00000004,   // Only needed on the server side
		PKG_CompiledIn					= 0x00000010,   // This package is from "compiled in" classes
		PKG_ForDiffing					= 0x00000020,	// This package was loaded just for the purposes of diffing
		PKG_EditorOnly					= 0x00000040,	// This is editor-only package (for example: editor module script package)
		PKG_Developer					= 0x00000080,	// Developer module
		PKG_ContainsMapData				= 0x00004000,   // Contains map data (UObjects only referenced by a single ULevel) but is stored in a different package
		PKG_Compiling					= 0x00010000,	// package is currently being compiled
		PKG_ContainsMap					= 0x00020000,	// Set if the package contains a ULevel/ UWorld object
		PKG_RequiresLocalizationGather	= 0x00040000,	// Set if the package contains any data to be gathered by localization
		PKG_PlayInEditor				= 0x00100000,	// Set if the package was created for the purpose of PIE
		PKG_ContainsScript				= 0x00200000,	// Package is allowed to contain UClass objects
		PKG_DisallowExport				= 0x00400000,	// Editor should not export asset in this package
		PKG_ReloadingForCooker			= 0x40000000,   // This package is reloading in the cooker, try to avoid getting data we will never need. We won't save this package
		PKG_FilterEditorOnly			= 0x80000000,	// Package has editor-only data filtered out
	};

	struct FGenerationInfo {
		int32	ExportCount;
		int32	NameCount;
		bool  read(istream_ref file) { return file.read(ExportCount, NameCount); }
	};

	int32	Tag;

	int32	FileVersionUE4;
	int32	FileVersionUE5;
	int32	FileVersionLicenseeUE4;
	FCustomVersionContainer CustomVersionContainer;

	int32	TotalHeaderSize;
	uint32	PackageFlags;
	FString FolderName;

	int32	NameCount;
	int32	NameOffset;
	FString LocalizationId;

	int32	GatherableTextDataCount;
	int32	GatherableTextDataOffset;			// Location into the file on disk for the gatherable text data items

	int32	ExportCount;
	int32	ExportOffset;						// Location into the file on disk for the ExportMap data

	int32	ImportCount;
	int32	ImportOffset;						// Location into the file on disk for the ImportMap data
	int32	DependsOffset;						// Location into the file on disk for the DependsMap data

	int32	SoftPackageReferencesCount;
	int32	SoftPackageReferencesOffset;		// Location into the file on disk for the soft package reference list

	int32	SearchableNamesOffset;				// Location into the file on disk for the SearchableNamesMap data
	int32	ThumbnailTableOffset;				// Thumbnail table offset

	FGuid	Guid;								// Current id for this package
	FGuid	PersistentGuid;						// Current persistent id for this package
//	FGuid	OwnerPersistentGuid;				// Package persistent owner for this package

	TArray<FGenerationInfo> Generations;		// Data about previous versions of this package

	FEngineVersion SavedByEngineVersion;		// Engine version this package was saved with
	FEngineVersion CompatibleWithEngineVersion;	// Engine version this package is compatible with. See SavedByEngineVersion

	uint32	CompressionFlags;					// Flags used to compress the file on save and uncompress on load
	uint32	PackageSource;						// Value that is used to determine if the package was saved by Epic (or licensee) or by a modder, etc

	int32	AssetRegistryDataOffset;			// Location into the file on disk for the asset registry tag data

	int64	BulkDataStartOffset;				// Offset to the location in the file where the bulkdata starts
	int32	WorldTileInfoDataOffset;			// Offset to the location in the file where the FWorldTileInfo data starts

	TArray<int32> ChunkIDs;						// Streaming install ChunkIDs

	int32	PreloadDependencyCount;
	int32	PreloadDependencyOffset;			// Location into the file on disk for the preload dependency data

	bool	read(istream_ref file);
};

bool FPackageFileSummary::read(istream_ref file) {
	file.read(Tag);

	if (Tag != PACKAGE_FILE_TAG && Tag != swap_endian(PACKAGE_FILE_TAG))
		return false;

	if (Tag == swap_endian(PACKAGE_FILE_TAG)) {
		Tag = PACKAGE_FILE_TAG;
	}

	/**
		* The package file version number when this package was saved.
		*
		* Lower 16 bits stores the UE3 engine version
		* Upper 16 bits stores the UE4/licensee version
		* For newer packages this is -7
		*		-2 indicates presence of enum-based custom versions
		*		-3 indicates guid-based custom versions
		*		-4 indicates removal of the UE3 version. Packages saved with this ID cannot be loaded in older engine versions
		*		-5 indicates the replacement of writing out the "UE3 version" so older versions of engine can gracefully fail to open newer packages
		*		-6 indicates optimizations to how custom versions are being serialized
		*		-7 indicates the texture allocation info has been removed from the summary
		*		-8 indicates that the UE5 version has been added to the summary
		*/
	const int32 CurrentLegacyFileVersion = -8;
	int32		LegacyFileVersion;

	if (!file.read(LegacyFileVersion) || LegacyFileVersion >= 0 || LegacyFileVersion < CurrentLegacyFileVersion)
		return false;	// This is probably an old UE3 file, make sure that the linker will fail to load with it

	if (LegacyFileVersion != -4) {
		int32 LegacyUE3Version = 0;
		file.read(LegacyUE3Version);
	}

	file.read(FileVersionUE4);

	if (LegacyFileVersion <= -8)
		file.read(FileVersionUE5);

	file.read(FileVersionLicenseeUE4);

	if (LegacyFileVersion <= -2)
		CustomVersionContainer.read(file, LegacyFileVersion);

	if (!FileVersionUE4 && !FileVersionLicenseeUE4)
		FileVersionUE4 = VER_UE4_AUTOMATIC_VERSION;

	file.read(TotalHeaderSize, FolderName, PackageFlags, NameCount, NameOffset);

	if (!(PackageFlags & PKG_FilterEditorOnly) && FileVersionUE4 >= VER_UE4_ADDED_PACKAGE_SUMMARY_LOCALIZATION_ID)
		file.read(LocalizationId);

	if (FileVersionUE4 >= VER_UE4_SERIALIZE_TEXT_IN_PACKAGES)
		file.read(GatherableTextDataCount, GatherableTextDataOffset);

	file.read(ExportCount, ExportOffset, ImportCount, ImportOffset, DependsOffset);

	if (FileVersionUE4 >= VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP)
		file.read(SoftPackageReferencesCount, SoftPackageReferencesOffset);

	if (FileVersionUE4 >= VER_UE4_ADDED_SEARCHABLE_NAMES)
		file.read(SearchableNamesOffset);

	file.read(ThumbnailTableOffset, Guid);

	if (!(PackageFlags & PKG_FilterEditorOnly) && FileVersionUE4 >= VER_UE4_ADDED_PACKAGE_OWNER) {
		file.read(PersistentGuid);
		if (FileVersionUE4 < VER_UE4_NON_OUTER_PACKAGE_IMPORT)
			file.get<FGuid>();// OwnerPersistentGuid
	} else
		PersistentGuid = Guid;

	Generations.read(file);

	if (FileVersionUE4 >= VER_UE4_ENGINE_VERSION_OBJECT)
		file.read(SavedByEngineVersion);

	if (FileVersionUE4 >= VER_UE4_PACKAGE_SUMMARY_HAS_COMPATIBLE_ENGINE_VERSION)
		file.read(CompatibleWithEngineVersion);

	file.read(CompressionFlags);

	TArray<FCompressedChunk> CompressedChunks;
	CompressedChunks.read(file);

	file.read(PackageSource);

	TArray<FString> AdditionalPackagesToCook;
	AdditionalPackagesToCook.read(file);

	if (LegacyFileVersion > -7) {
		int32 NumTextureAllocations = 0;
		file.read(NumTextureAllocations);
	}

	file.read(AssetRegistryDataOffset);
	file.read(BulkDataStartOffset);

	if (FileVersionUE4 >= VER_UE4_WORLD_LEVEL_INFO)
		file.read(WorldTileInfoDataOffset);

	if (FileVersionUE4 >= VER_UE4_CHANGED_CHUNKID_TO_BE_AN_ARRAY_OF_CHUNKIDS) {
		ChunkIDs.read(file);

	} else if (FileVersionUE4 >= VER_UE4_ADDED_CHUNKID_TO_ASSETDATA_AND_UPACKAGE) {
		int ChunkID;
		if (file.read(ChunkID) && ChunkID >= 0)
			ChunkIDs.push_back(ChunkID);
	}

	if (FileVersionUE4 >= VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS)
		file.read(PreloadDependencyCount, PreloadDependencyOffset);

	return true;
}

struct FBulkData {
	enum EBulkDataFlags {
		None							= 0,			
		PayloadAtEndOfFile				= 1 << 0,		// If set, payload is stored at the end of the file and not inline
		SerializeCompressedZLIB			= 1 << 1,		// If set, payload should be [un]compressed using ZLIB during serialization
		ForceSingleElementSerialization	= 1 << 2,		// Force usage of SerializeElement over bulk serialization
		SingleUse						= 1 << 3,		// Bulk data is only used once at runtime in the game
		Unused							= 1 << 5,		// Bulk data won't be used and doesn't need to be loaded
		ForceInlinePayload				= 1 << 6,		// Forces the payload to be saved inline, regardless of its size
		ForceStreamPayload				= 1 << 7,		// Forces the payload to be always streamed, regardless of its size
		PayloadInSeperateFile			= 1 << 8,		// If set, payload is stored in a .upack file alongside the uasset
		SerializeCompressedBitWindow	= 1 << 9,		// DEPRECATED: If set, payload is compressed using platform specific bit window
		Force_NOT_InlinePayload			= 1 << 10,		// There is a new default to inline unless you opt out
		OptionalPayload					= 1 << 11,		// This payload is optional and may not be on device
		MemoryMappedPayload				= 1 << 12,		// This payload will be memory mapped, this requires alignment, no compression etc
		Size64Bit						= 1 << 13,		// Bulk data size is 64 bits long
		DuplicateNonOptionalPayload		= 1 << 14,		// Duplicate non-optional payload in optional bulk data
		BadDataVersion					= 1 << 15,		// Indicates that an old ID is present in the data, at some point when the DDCs are flushed we can remove this
	};
	uint32	flags;
	uint64	element_count;
	uint64	disk_size;
	uint64	file_offset;

	bool	seperate_file() const {
		return !!(flags & PayloadInSeperateFile);
	}

	bool	read(istream_ref file) {
		file.read(flags);
		if (flags & Size64Bit) {
			file.read(element_count);
			file.read(disk_size);
		} else {
			element_count	= file.get<uint32>();
			disk_size		= file.get<uint32>();
		}
		file.read(file_offset);
		if (flags & ForceInlinePayload) {
			ISO_ASSERT(file_offset == file.tell());
			file.seek_cur(disk_size);
		}
		return true;
	}

	streamptr		offset(streamptr bulk_offset) const {
		return flags & PayloadAtEndOfFile ? bulk_offset + file_offset : file_offset;
	}

	malloc_block	data(istream_ref file, streamptr bulk_offset) const {
		auto		pos		= make_save_pos(file);
		file.seek(offset(bulk_offset));

		if (flags & SerializeCompressedZLIB) {
			auto	PackageFileTag	= file.get<FCompressedChunkInfo>();
			auto	Summary			= file.get<FCompressedChunkInfo>();
			bool	swapped			= PackageFileTag.CompressedSize != FPackageFileSummary::PACKAGE_FILE_TAG;

			int64	chunk_size		= PackageFileTag.UncompressedSize;
			int		nchunks			= div_round_up(Summary.UncompressedSize, chunk_size);

			temp_array<FCompressedChunkInfo>	chunks(nchunks);
			file.read(chunks);

			malloc_block	mem(Summary.UncompressedSize);
			mem.fill(0xaaaaaaaa_u32);
			uint8			*p	= mem;
			auto			fp	= file.tell();
			for (auto &i : chunks) {
				file.seek(fp);
				zlib_reader(file, i.UncompressedSize).readbuff(p, i.UncompressedSize);
				p	+= i.UncompressedSize;
				fp	+= i.CompressedSize;
			}
			return move(mem);
		}
		return malloc_block(file, disk_size);
	}

	istream_ptr		reader(istream_ref file, streamptr bulk_offset) const {
		if (flags & SerializeCompressedZLIB)
			return new reader_mixout<memory_reader_owner>(move(data(file, bulk_offset)));
		return new istream_offset(copy(file), offset(bulk_offset), disk_size);
	}
};


enum CompressionFormat : uint8 {
	//case 0:
	//{
	//	// can't rely on archive serializing FName, so use String
	//	FString LoadedString;
	//	Archive << LoadedString;
	//	Compressor = FName(*LoadedString);
	//	break;
	//}
	COMP_NONE	= 1,
	COMP_Oodle	= 2,
	COMP_Zlib	= 3,
	COMP_Gzip	= 4,
	COMP_LZ4	= 5,
};

static constexpr uint64 DefaultBlockSize = 256 * 1024;
static constexpr uint64 DefaultHeaderSize = 4 * 1024;

struct FBlake3Hash {
	union {
		uint32	_;//align
		uint8	array[32];
	};
};

struct FHeader {
	enum EMethod : uint8 {
		None	= 0,	/** Header is followed by one uncompressed block. */
		Oodle	= 3,	/** Header is followed by an array of compressed block sizes then the compressed blocks. */
		LZ4		= 4,	/** Header is followed by an array of compressed block sizes then the compressed blocks. */
	};
	static constexpr uint32 ExpectedMagic = 0xb7756362; // <dot>ucb
	uint32be	Magic				= ExpectedMagic;
	uint32be	Crc32				= 0;
	EMethod		Method				= None;
	uint8		Compressor			= 0;
	uint8		CompressionLevel	= 0;
	uint8		BlockSizeExponent	= 0;
	uint32be	BlockCount			= 0;
	uint64be	TotalRawSize		= 0;
	uint64be	TotalCompressedSize	= 0;
	FBlake3Hash RawHash;
};

struct FEditorBulkData {
	enum EFlags : uint32 {
		None						= 0,
		IsVirtualized				= 1 << 0,
		HasPayloadSidecarFile		= 1 << 1,
		ReferencesLegacyFile		= 1 << 2,
		LegacyFileIsCompressed		= 1 << 3,
		DisablePayloadCompression	= 1 << 4,
		LegacyKeyWasGuidDerived		= 1 << 5,
		HasRegistered				= 1 << 6,
		IsTornOff					= 1 << 7,
		ReferencesWorkspaceDomain	= 1 << 8,
		StoredInPackageTrailer		= 1 << 9,
		TransientFlags				= HasRegistered | IsTornOff,
	};

	enum EPackageSegment : uint8 {
		Header,
		Exports,
		BulkDataDefault,
		BulkDataOptional,
		BulkDataMemoryMapped,
		PayloadSidecar,
	};
	
	EFlags			flags	= EFlags::None;
	FGuid			BulkDataId;
	uint8			PayloadContentId[20];
	int64			OffsetInFile	= -1;
	int64			PayloadSize		= 0;

	bool	read(istream_ref file) {
		//const FPackageTrailer* Trailer = GetTrailerFromOwner(Owner);

		file.read(flags, BulkDataId, PayloadContentId, PayloadSize);
		if (flags & StoredInPackageTrailer)
			OffsetInFile = -1;//Trailer->FindPayloadOffsetInFile(PayloadContentId);
		else if (!(flags & IsVirtualized))
			file.read(OffsetInFile);

	#if 0
		EPackageSegment	PackageSegment = Header;

		if (!(flags & IsVirtualized)) {
			// If we can lazy load then find the PackagePath, otherwise we will want to serialize immediately.
			FArchive* CacheableArchive = Ar.GetCacheableArchive();

			FCompressedBuffer CompressedPayload;
			SerializeData(Ar, CompressedPayload, flags);

			if (CompressedPayload.GetRawSize() > 0)
				Payload = CompressedPayload.Decompress();
			else
				Payload.clear();
		}
	#endif
		return true;
	}

	malloc_block		data(istream_ref file) const {
		auto	pos	= make_save_pos(file);
		file.seek(OffsetInFile);

		FHeader	header;
		file.read(header);
		malloc_block	raw(header.TotalRawSize);

		switch (header.Method) {
			case FHeader::None:
				raw.read(file, header.TotalRawSize);
				return raw;

			case FHeader::Oodle: {
				uint64	block_size			= uint64(1) << header.BlockSizeExponent;
				int		num_blocks			= div_round_up(header.TotalRawSize, block_size);
				temp_array<uint32be>	blocks(file, num_blocks);
				uint8	*out	= raw;
				for (uint32 i : blocks) {
					temp_block	in(file, i);
					int r = oodle::decompress(in, in.size(), out, block_size);
					out	+= block_size;
				}
				return raw;
			}

			case FHeader::LZ4:
				return malloc_block(file, header.TotalCompressedSize);
		}
		/*

		uint32	ARCHIVE_V1_HEADER_TAG	= FPackageFileSummary::PACKAGE_FILE_TAG;
		uint64	ARCHIVE_V2_HEADER_TAG	= ARCHIVE_V1_HEADER_TAG | ((uint64)0x22222222<<32);

		auto	PackageFileTag	= file.get<FCompressedChunkInfo>();
		CompressionFormat CompressionFormatToDecode = COMP_Zlib;

		bool	swapped	= false;
		uint64	tag		= PackageFileTag.CompressedSize;
		if (tag == ARCHIVE_V1_HEADER_TAG || tag == swap_endian(ARCHIVE_V1_HEADER_TAG) || tag == swap_endian((uint64)ARCHIVE_V1_HEADER_TAG)) {
			swapped = tag != ARCHIVE_V1_HEADER_TAG;

		} else if ( tag == ARCHIVE_V2_HEADER_TAG || tag == swap_endian(ARCHIVE_V2_HEADER_TAG)) {
			swapped = tag != ARCHIVE_V2_HEADER_TAG;
			file.read(CompressionFormatToDecode);

		} else {
			//return false;
		}

		auto	Summary			= file.get<FCompressedChunkInfo>();

		int64	chunk_size		= PackageFileTag.UncompressedSize;
		int		nchunks			= div_round_up(Summary.UncompressedSize, chunk_size);

		temp_array<FCompressedChunkInfo>	chunks(nchunks);
		file.read(chunks);

		malloc_block	mem(Summary.UncompressedSize);
		mem.fill(0xaaaaaaaa_u32);
		uint8			*p	= mem;
		auto			fp	= file.tell();
		for (auto &i : chunks) {
			file.seek(fp);
			zlib_reader(file, i.UncompressedSize).readbuff(p, i.UncompressedSize);
			p	+= i.UncompressedSize;
			fp	+= i.CompressedSize;
		}

		return new reader_mixout<memory_reader_owner>(move(mem));
		*/
	}


};

// UObject resource type for objects that are referenced by this package, but contained within another package
struct FObjectImport : public FObjectResource {
	FName			ClassPackage;
	FName			ClassName;
	FName			PackageName;
	bool32			bImportOptional;
};

// UObject resource type for objects that are contained within this package and can be referenced by other packages
struct FObjectExport : public FObjectResource {
	FPackageIndex	ClassIndex;
	FPackageIndex	SuperIndex;
	FPackageIndex	TemplateIndex;
	EObjectFlags	ObjectFlags;

	packed<int64>	SerialSize, SerialOffset;
	bool32			bForcedExport;
	bool32			bNotForClient;
	bool32			bNotForServer;
	bool32			bNotAlwaysLoadedForEditorGame;
	bool32			bIsAsset;
	bool32			bIsInheritedInstance;
	bool32			bGeneratePublicHash;

//	FGuid			PackageGuid;
	uint32			PackageFlags;

	int32			FirstExportDependency;
	int32			SerializationBeforeSerializationDependencies;
	int32			CreateBeforeSerializationDependencies;
	int32			SerializationBeforeCreateDependencies;
	int32			CreateBeforeCreateDependencies;

	bool read(istream_ref file, EUnrealEngineObjectUE4Version ver) {
		file.read(ClassIndex);
		file.read(SuperIndex);

		if (ver >= VER_UE4_TemplateIndex_IN_COOKED_EXPORTS)
			file.read(TemplateIndex);

		file.read(OuterIndex);
		file.read(ObjectName);

		ObjectFlags = EObjectFlags(file.get<uint32>() & RF_Load);

		if (ver < VER_UE4_64BIT_EXPORTMAP_SERIALSIZES) {
			SerialSize		= file.get<uint32>();
			SerialOffset	= file.get<uint32>();
		} else {
			file.read(SerialSize);
			file.read(SerialOffset);
		}

		file.read(bForcedExport, bNotForClient, bNotForServer);
			
		if (ver < VER_UE5_REMOVE_OBJECT_EXPORT_PACKAGE_GUID)
			file.get<FGuid>();

		if (ver >= VER_UE5_TRACK_OBJECT_EXPORT_IS_INHERITED)
			file.read(bIsInheritedInstance);

		file.read(PackageFlags);

		if (ver >= VER_UE4_LOAD_FOR_EDITOR_GAME)
			file.read(bNotAlwaysLoadedForEditorGame);

		if (ver >= VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT)
			file.read(bIsAsset);

		if (ver >= VER_UE5_OPTIONAL_RESOURCES)
			file.read(bGeneratePublicHash);

		if (ver >= VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS) {
			iso::read(file,
				FirstExportDependency,
				SerializationBeforeSerializationDependencies,
				CreateBeforeSerializationDependencies,
				SerializationBeforeCreateDependencies,
				CreateBeforeCreateDependencies
			);
		}
		return true;
	}
};

struct FLinkerTables {
	bool							fixed_names;
	FCustomVersionContainer			CustomVersionContainer;
	int64							BulkDataStartOffset;
	TArray<FString>					NameMap;
	TArray<FObjectImport>			ImportMap;
	TArray<FObjectExport>			ExportMap;
	TArray<TArray<FPackageIndex>>	DependsMap;
	TArray<FName>					SoftPackageReferenceList;
	TMap<FPackageIndex, TArray<FName>> SearchableNamesMap;

	FLinkerTables() : fixed_names(false), BulkDataStartOffset(0) {}
	
	FLinkerTables(istream_ref file, const FPackageFileSummary& sum) : fixed_names(true),
		CustomVersionContainer(sum.CustomVersionContainer),
		BulkDataStartOffset(sum.BulkDataStartOffset),
		ImportMap(sum.ImportCount)
	{
		istream_linker	file2(file, none, this);

		if (sum.NameCount > 0) {
			file.seek(sum.NameOffset);
			NameMap.resize(sum.NameCount);
			for (auto &i : NameMap) {
				FNameEntrySerialized	name;
				name.read(file, sum.FileVersionUE4 >= VER_UE4_NAME_HASHES_SERIALIZED);
				i = name;
			}
		}

		if (sum.ImportCount > 0) {
			file.seek(sum.ImportOffset);
			for (auto &i : ImportMap) {
				bool ret = file.read(i.ClassPackage, i.ClassName, i.OuterIndex, i.ObjectName);
				if (sum.FileVersionUE4 >= VER_UE4_NON_OUTER_PACKAGE_IMPORT)
					file.read(i.PackageName);

				if (sum.FileVersionUE5 >= VER_UE5_OPTIONAL_RESOURCES)
					file.read(i.bImportOptional);

			}
		}

		if (sum.ExportCount > 0) {
			file.seek(sum.ExportOffset);
			ExportMap.resize(sum.ExportCount);
			for (auto &i : ExportMap)
				i.read(file, (EUnrealEngineObjectUE4Version)sum.FileVersionUE5);
		}


		if (sum.FileVersionUE4 >= VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP && sum.SoftPackageReferencesOffset > 0 && sum.SoftPackageReferencesCount > 0) {
			file.seek(sum.SoftPackageReferencesOffset);

			if (sum.FileVersionUE4 < VER_UE4_ADDED_SOFT_OBJECT_PATH) {
				for (int32 ReferenceIdx = 0; ReferenceIdx < sum.SoftPackageReferencesCount; ++ReferenceIdx) {
					FString PackageName;
					file.read(PackageName);
				}
			} else {
				SoftPackageReferenceList.read(file, sum.SoftPackageReferencesCount);
			}
		}

		if (sum.FileVersionUE4 >= VER_UE4_ADDED_SEARCHABLE_NAMES && sum.SearchableNamesOffset > 0) {
			file.seek(sum.SearchableNamesOffset);
			file.read(SearchableNamesMap);
		}
	}

	void	read_names(istream_ref file, int num, bool hashes) {
		NameMap.resize(num);
		for (auto &i : NameMap) {
			FNameEntrySerialized	name;
			name.read(file, hashes);
			i = name;
		}
		fixed_names = true;
	}

	cstring	lookup(const FName& name) const {
		if (~name.ComparisonIndex) {
			if (name.ComparisonIndex < NameMap.size())
				return (const char*)NameMap[name.ComparisonIndex];
			ISO_TRACE("Bad name\n");
		}
		return "None";
	}

	const FObjectResource	*lookup(FPackageIndex index) const {
		if (index.IsImport())
			return index.ToImport() < ImportMap.size() ? &ImportMap[index.ToImport()] : 0;
		if (index.IsExport())
			return index.ToExport() < ExportMap.size() ? &ExportMap[index.ToExport()] : 0;
		return nullptr;
	}

	FName	add_name(const FString &name) {
		NameMap.push_back(name);
		return {NameMap.size() - 1, 0};
	}
	FPackageIndex	add_package(FObjectImport &&obj) {
		ImportMap.push_back(move(obj));
		return {ImportMap.size() - 1, true};
	}

	int	CustomVer(const FGuid& Key) {
		if (auto i = CustomVersionContainer.Get(Key))
			return i->Version;
		return -1;
	}
};

int64 istream_linker::bulk_data() {
	return linker->BulkDataStartOffset;
}

cstring istream_linker::lookup(const FName& name) const {
	return linker ? linker->lookup(name) : "None";
}

const FObjectResource* istream_linker::lookup(FPackageIndex index) const {
	return linker ? linker->lookup(index) : nullptr;
}

bool read(istream_linker &file, FName& name) {
	if (file.linker->fixed_names)
		return iso::read((reader_ref<istream_ref>&)file, name);

	name = file.linker->add_name(file.get<FString>());
	return true;
}

bool read(istream_linker &file, FPackageIndex& pkg) {
	if (file.linker->fixed_names)
		return iso::read((reader_ref<istream_ref>&)file, pkg);

	FObjectImport	obj;
	file.read(obj.ObjectName);
	pkg = file.linker->add_package(move(obj));
	return true;
}


template<typename T> struct TBulkArrayData : dynamic_array<T> {
	bool	read(istream_linker &file) {
		auto	bulk	= file.get<FBulkData>();
		auto	fp		= make_save_pos(file);
		dynamic_array<T>::read(bulk.reader(file, file.bulk_data()), bulk.element_count);
		return true;
	}
};

struct FPropertyTag {
	FName	Type;					// Type of property
	uint8	BoolVal			= 0;	// a boolean property's value (never need to serialize data for bool properties except here)
	FName	Name;					// Name of property
	FName	StructName;				// Struct name if FStructProperty
	FName	EnumName;				// Enum name if FByteProperty or FEnumProperty
	FName	InnerType;				// Inner type if FArrayProperty, FSetProperty, or FMapProperty
	FName	ValueType;				// Value type if UMapPropery
	int32	Size			= 0;	// Property size
	int32	ArrayIndex		= -1;	// Index if an array; else 0
	int64	SizeOffset		= -1;	// location in stream of tag size member
	FGuid	StructGuid;
	uint8	HasPropertyGuid	= 0;
	FGuid	PropertyGuid;

	bool read(istream_linker& file) {
		*this = FPropertyTag();

		if (!file.read(Name) || file.lookup(Name) == "None")
			return false;

		if (!file.read(Type, Size, ArrayIndex))
			return false;

		if (Type.Number == 0) {
			switch (string_hash(file.lookup(Type))) {
				case "StructProperty"_hash:
					file.read(StructName);
					file.read(StructGuid);
					break;

				case "BoolProperty"_hash:
					file.read(BoolVal);
					break;

				case "ByteProperty"_hash:
				case "EnumProperty"_hash:
					file.read(EnumName);
					break;

				case "ArrayProperty"_hash:
				case "SetProperty"_hash:
					file.read(InnerType);
					break;

				case "MapProperty"_hash:
					file.read(InnerType);
					file.read(ValueType);
					break;
			}
		}
		file.read(HasPropertyGuid);
		if (HasPropertyGuid)
			file.read(PropertyGuid);
		return true;
	}
	FPropertyTag inner() const {
		FPropertyTag	tag = *this;
		tag.Type		= tag.InnerType;
		tag.InnerType	= FName();
		return tag;
	}
	FPropertyTag value() const {
		FPropertyTag	tag = *this;
		tag.Type		= tag.ValueType;
		tag.ValueType	= FName();
		return tag;
	}
};

struct ObjectLoader : static_hash<ObjectLoader, const char*> {
	typedef ISO_ptr<void>	(load_t)(istream_linker&);
	load_t			*load;
	ObjectLoader(const char *name,  load_t *load) : base(name), load(load) {}
};

template<typename T> struct ObjectLoaderRaw : ObjectLoader {
	ObjectLoaderRaw(const char *name) : ObjectLoader(name, [](istream_linker &file)->ISO_ptr<void> {
		return ISO::MakePtr(0, file.get<T>());
	}) {}
};


struct FSoftClassPath : FSoftObjectPath {};

ObjectLoaderRaw<FGuid>				load_Guid("Guid");
ObjectLoaderRaw<FVector>			load_Vector("Vector");
ObjectLoaderRaw<FVector2D>			load_Vector2D("Vector2D");
ObjectLoaderRaw<FVector4>			load_Vector4("Vector4");
ObjectLoaderRaw<FQuat>				load_Quat("Quat");
ObjectLoaderRaw<FIntPoint>			load_IntPoint("IntPoint");
ObjectLoaderRaw<FIntVector>			load_IntVector("IntVector");
ObjectLoaderRaw<FRotator>			load_Rotator("Rotator");
ObjectLoaderRaw<FColor>				load_Color("Color");
ObjectLoaderRaw<FLinearColor>		load_LinearColor("LinearColor");
ObjectLoaderRaw<FBox>				load_Box("Box");
ObjectLoaderRaw<FSphere>			load_Sphere("Sphere");
ObjectLoaderRaw<FSoftObjectPath>	load_SoftObjectPath("SoftObjectPath");
ObjectLoaderRaw<FSoftClassPath>		load_SoftClassPath("SoftClassPath");

hash_map<const char*, const char*>	known_ids = {
	{"PropertyGuids", "Guid"},
	{"UserParameterRedirects", "FNiagaraVariable"},
};

int	element_size(const char* type) {
	switch (string_hash(type)) {
		case "Int8Property"_fnv:	return (int)sizeof(int8);
		case "Int16Property"_fnv:	return (int)sizeof(int16);
		case "IntProperty"_fnv:		return (int)sizeof(int32);
		case "Int64Property"_fnv:	return (int)sizeof(int64);
		case "UInt16Property"_fnv:	return (int)sizeof(uint16);
		case "UInt32Property"_fnv:	return (int)sizeof(uint32);
		case "UInt64Property"_fnv:	return (int)sizeof(uint64);
		case "FloatProperty"_fnv:	return (int)sizeof(float);
		case "DoubleProperty"_fnv:	return (int)sizeof(double);
		case "ObjectProperty"_fnv:	return (int)sizeof(FPackageIndex);
		case "NameProperty"_fnv:	return (int)sizeof(FName);
		case "BoolProperty"_fnv:	return 1;
		case "StrProperty"_fnv:		return -1;
		default: return 0;
	}
}

void ReadTagged(istream_linker& file, anything &tagged);

ISO_ptr<void> ReadTag(istream_linker& file, const FPropertyTag &tag, ISO::tag id) {
	switch (string_hash(file.lookup(tag.Type))) {
		case "Int8Property"_fnv:
			return ISO::MakePtr(id, file.get<int8>());

		case "Int16Property"_fnv:
			return ISO::MakePtr(id, file.get<int16>());

		case "IntProperty"_fnv:
			return ISO::MakePtr(id, file.get<int32>());

		case "Int64Property"_fnv:
			return ISO::MakePtr(id, file.get<int64>());

		case "ByteProperty"_fnv:
			if (id && file.lookup(tag.EnumName) == "None")
				return ISO::MakePtr(id, file.get<int8>());
			else
				return ISO::MakePtr<string>(id, file.lookup(file.get<FName>()));

		case "EnumProperty"_fnv:
			return ISO::MakePtr<string>(id, file.lookup(file.get<FName>()));

		case "UInt16Property"_fnv:
			return ISO::MakePtr(id, file.get<uint16>());

		case "UInt32Property"_fnv:
			return ISO::MakePtr(id, file.get<uint32>());

		case "UInt64Property"_fnv:
			return ISO::MakePtr(id, file.get<uint64>());

		case "FloatProperty"_fnv:
			return ISO::MakePtr(id, file.get<float>());

		case "DoubleProperty"_fnv:
			return ISO::MakePtr(id, file.get<double>());

		case "StructProperty"_fnv: {
			auto	struct_name = file.lookup(tag.StructName);
			if (struct_name == "None")
				struct_name = known_ids[id].or_default();

			if (auto i = ObjectLoader::get(struct_name)) {
				auto	p = i->load(file);
				p.SetID(id);
				return p;
			}
			ISO_TRACEFI("\tLoading unknown struct %0\n", file.lookup(tag.StructName));
			ISO_ptr<anything>	tagged(id);
			ReadTagged(file, *tagged);
			return tagged;
		}

		case "ObjectProperty"_fnv: {
			auto	i = file.get<FPackageIndex>();
			if (auto obj = file.lookup(i))
				return ISO::MakePtr(id, obj->p);
			return ISO_NULL;
		}

		case "BoolProperty"_fnv:
			return ISO::MakePtr(id, !!(id ? tag.BoolVal : file.get<uint8>()));

		case "StrProperty"_fnv:
			return ISO::MakePtr(id, file.get<FString>());

		case "NameProperty"_fnv:
			return ISO::MakePtr(id, file.get<FName2>());

		case "ArrayProperty"_fnv: {
			auto	size	= file.get<int>();
			auto	inner	= tag.inner();

			switch (string_hash(file.lookup(inner.Type))) {
				case "Int8Property"_fnv:	return ISO::MakePtr(id, ISO::OpenArray<int8>(file, size));
				case "Int16Property"_fnv:	return ISO::MakePtr(id, ISO::OpenArray<int16>(file, size));
				case "IntProperty"_fnv:		return ISO::MakePtr(id, ISO::OpenArray<int32>(file, size));
				case "Int64Property"_fnv:	return ISO::MakePtr(id, ISO::OpenArray<int64>(file, size));
				case "UInt16Property"_fnv:	return ISO::MakePtr(id, ISO::OpenArray<uint16>(file, size));
				case "UInt32Property"_fnv:	return ISO::MakePtr(id, ISO::OpenArray<uint32>(file, size));
				case "UInt64Property"_fnv:	return ISO::MakePtr(id, ISO::OpenArray<uint64>(file, size));
				case "FloatProperty"_fnv:	return ISO::MakePtr(id, ISO::OpenArray<float>(file, size));
				case "DoubleProperty"_fnv:	return ISO::MakePtr(id, ISO::OpenArray<double>(file, size));
				case "NameProperty"_fnv:	return ISO::MakePtr(id, ISO::OpenArray<FName>(file, size));
				case "ByteProperty"_fnv:
				case "BoolProperty"_fnv:	return ISO::MakePtr(id, ISO::OpenArray<uint8>(file, size));
				case "StrProperty"_fnv:		return ISO::MakePtr(id, ISO::OpenArray<FString>(file, size));
				case "StructProperty"_fnv:
					file.read(inner);
				default:
					break;
			}

			ISO_ptr<anything>	array(id);
			for (int i = 0; i < size; i++)
				array->Append(ReadTag(file, inner, none));
			return array;
		}
		case "SetProperty"_fnv: {
			ISO_ptr<anything>	array(id);
			auto	size	= file.get<int>();
			auto	inner	= tag.inner();

			if (auto nremove = file.get<int>()) {
				ISO_ptr<anything>	remove("remove");
				array->Append(remove);
				for (int i = 0; i < nremove; i++)
					remove->Append(ReadTag(file, inner, none));
			}

			for (int i = 0; i < size; i++)
				array->Append(ReadTag(file, inner, none));
			return array;

		}
		case "MapProperty"_fnv: {
			ISO_ptr<anything>	array(id);
			auto	tag_end		= file.tell() + tag.Size;
			auto	inner		= tag.inner();
			auto	value		= tag.value();

			if (auto nremove = file.get<int>()) {
				ISO_ptr<anything>	remove("remove");
				array->Append(remove);
				for (int i = 0; i < nremove; i++)
					remove->Append(ReadTag(file, inner, none));
			}

			auto	count		= file.get<int>();
			int		key_size	= element_size(file.lookup(inner.Type));
			int		val_size	= element_size(file.lookup(value.Type));
			bool	known_val	= val_size != 0;
			if (!known_val && count && key_size > 0)
				val_size = (tag.Size - 8) / count - key_size;

			for (int i = 0; i < count; i++) {
				ISO_ptr<pair<ISO_ptr<void>, ISO_ptr<void>>>	p(0);
				p->a = ReadTag(file, inner, none);
				if (true || known_val) {
					p->b = ReadTag(file, value, none);
				} else {
					if (val_size == 0 && i == count - 1)
						val_size = tag_end - file.tell();
					ISO_ptr<ISO_openarray<uint8>>	m(id, val_size);
					file.readbuff(*m, val_size);
					p->b = m;
				}
				array->Append(p);
			}
			return array;
		}
		case "MulticastSparseDelegateProperty"_fnv:
		case "MulticastInlineDelegateProperty"_fnv:
		case "TextProperty"_fnv:
		case "SoftObjectProperty"_fnv:
		case "DelegateProperty"_fnv:
		case "InterfaceProperty"_fnv:
		case "FieldPathProperty"_fnv:
			file.seek_cur(tag.Size);
			return ISO_NULL;

		default:
			break;
	}
	ISO_ASSERT(0);
	file.seek_cur(tag.Size);
	return ISO_NULL;
}

void ReadTagged(istream_linker& file, anything &tagged) {
	FPropertyTag	tag;
	while (tag.read(file)) {
		if (tag.Type.Number == 0) {
			auto	tag_end = file.tell() + tag.Size;
			auto	p		= ReadTag(file, tag, file.lookup(tag.Name));
			if (file.tell() != tag_end) {
				ISO_TRACEF("Missing read:") << tag_end - file.tell() << '\n';
				file.seek(tag_end);
			}
			if (p)
				tagged.Append(p);
		}
	}
}

bool ReadTagged(istream_linker& file, ISO::Browser2 b);

bool ReadTag(istream_linker& file, const FPropertyTag &tag, ISO::Browser2 b) {
	switch (string_hash(file.lookup(tag.Type))) {
		case "Int8Property"_fnv:
			return b.Set(file.get<int8>());

		case "Int16Property"_fnv:
			return b.Set(file.get<int16>());

		case "IntProperty"_fnv:
			return b.Set(file.get<int32>());

		case "Int64Property"_fnv:
			return b.Set(file.get<int64>());

		case "ByteProperty"_fnv:
			if (file.lookup(tag.EnumName) == "None")
				return b.Set(file.get<int64>());
			else
				return b.Set((const char*)file.lookup(file.get<FName>()));

		case "EnumProperty"_fnv:
			return b.Set((const char*)file.lookup(file.get<FName>()));

		case "UInt16Property"_fnv:
			return b.Set(file.get<uint16>());

		case "UInt32Property"_fnv:
			return b.Set(file.get<uint32>());

		case "UInt64Property"_fnv:
			return b.Set(file.get<uint64>());

		case "FloatProperty"_fnv:
			return b.Set(file.get<float>());

		case "DoubleProperty"_fnv:
			return b.Set(file.get<double>());

		case "StructProperty"_fnv:
			return ReadTagged(file, b);

		case "ObjectProperty"_fnv:
			if (auto obj = file.lookup(file.get<FPackageIndex>()))
				return b.Set(obj->p);
			return false;

		case "BoolProperty"_fnv:
			return b.Set(!!file.get<uint8>());

		case "StrProperty"_fnv:
			return b.Set(get(file.get<FString>()));

		case "NameProperty"_fnv:
			return b.Set((const char*)file.get<FName2>());

		case "ArrayProperty"_fnv: {
			auto	size	= file.get<int>();
			auto	inner	= tag.inner();
			b.Resize(size);

			for (int i = 0; i < size; i++) {
				if (!ReadTag(file, inner, b[i]))
					return false;
			}
			return true;
		}
		case "SetProperty"_fnv: {
			auto	size	= file.get<int>();
			auto	inner	= tag.inner();

			if (auto nremove = file.get<int>()) {
				for (int i = 0; i < nremove; i++)
					ReadTag(file, inner, none);
			}

			b.Resize(size);
			for (int i = 0; i < size; i++) {
				if (!ReadTag(file, inner, b[i]))
					return false;
			}
			 return true;
		}
		case "MapProperty"_fnv: {
			auto	inner	= tag.inner();
			auto	value	= tag.value();

			if (auto nremove = file.get<int>()) {
				for (int i = 0; i < nremove; i++)
					ReadTag(file, inner, none);
			}

			auto	size	= file.get<int>();
			b.Resize(size);
			for (int i = 0; i < size; i++) {
				if (!ReadTag(file, inner, b[i]["a"]) || !ReadTag(file, value, b[i]["b"]))
					return false;
			}
			return true;
		}
		case "MulticastSparseDelegateProperty"_fnv:
		case "MulticastInlineDelegateProperty"_fnv:
		case "TextProperty"_fnv:
		case "SoftObjectProperty"_fnv:
		case "DelegateProperty"_fnv:
		case "InterfaceProperty"_fnv:
		case "FieldPathProperty"_fnv:
			return false;

		default:
			break;
	}
	ISO_ASSERT(0);
	return false;
}


bool ReadTagged(istream_linker& file, ISO::Browser2 b) {
	anything		*a = b;
	FPropertyTag	tag;
	while (tag.read(file)) {
		if (tag.Type.Number == 0) {
			auto	tag_end = file.tell() + tag.Size;
			auto	id		= file.lookup(tag.Name);

			if (auto b2 = b.ref(id)) {
				ReadTag(file, tag, b2);

			} else {
				auto	p	= ReadTag(file, tag, id);
				if (p)
					a->Append(p);
			}
			if (file.tell() != tag_end) {
				ISO_TRACEF("Missing read:") << tag_end - file.tell() << '\n';
				file.seek(tag_end);
			}
		}
	}
	return true;
}

//-----------------------------------------------------------------------------
//	properties
//-----------------------------------------------------------------------------

bool _InlinePtr::read(istream_linker& file) {
	auto	type	= file.get<FName>();
	if (auto i = ObjectLoader::get(file.lookup(type))) {
		*((ISO_ptr<void>*)this) = i->load(file);
	} else {
		ISO_TRACEF("Needs ") << file.lookup(type) << '\n';
		ISO_ASSERT(0);
	}
	return true;
}

ISO_DEFUSER(FProperty::EPropertyFlags, uint64);
ISO_DEFUSER(FProperty::ELifetimeCondition, uint8);
ISO_DEFUSERCOMPV(FField, NamePrivate, FlagsPrivate, MetaDataMap);
ISO_DEFUSERCOMPBV(FProperty, FField, ArrayDim,ElementSize,PropertyFlags,RepIndex,RepNotifyFunc,BlueprintReplicationCondition);
ISO_DEFUSERCOMPBV(FObjectProperty, FProperty, PropertyClass);
ISO_DEFUSERCOMPBV(FClassProperty, FObjectProperty, MetaClass);
ISO_DEFUSERCOMPBV(FStructProperty, FProperty, Struct);
ISO_DEFUSERCOMPBV(FBoolProperty, FProperty, FieldSize,ByteOffset,ByteMask,FieldMask);
ISO_DEFUSERCOMPBV(FByteProperty, FProperty, unknown);
ISO_DEFUSERCOMPBV(FDelegateProperty, FProperty, SignatureFunction);
ISO_DEFUSERCOMPBV(FArrayProperty, FProperty, Inner);
ISO_DEFUSERCOMPBV(FSetProperty, FProperty, ElementProp);
ISO_DEFUSERCOMPBV(FMapProperty, FProperty, KeyProp, ValueProp);
ISO_DEFUSERCOMPBV(FFieldPathProperty, FProperty, PropertyClass);
ISO_DEFUSERCOMPBV(FInterfaceProperty, FProperty, InterfaceClass);

template<typename T> struct ISO::def<TProperty_Numeric<T>> : ISO::def<FProperty> {};

ObjectLoaderRaw<FObjectProperty>					load_ObjectProperty						("ObjectProperty");
ObjectLoaderRaw<FClassProperty>						load_ClassProperty						("ClassProperty");
ObjectLoaderRaw<FStructProperty>					load_StructProperty						("StructProperty");
ObjectLoaderRaw<FNameProperty>						load_NameProperty						("NameProperty");
ObjectLoaderRaw<FStrProperty>						load_StrProperty						("StrProperty");
ObjectLoaderRaw<FTextProperty>						load_TextProperty						("TextProperty");
ObjectLoaderRaw<FBoolProperty>						load_BoolProperty						("BoolProperty");
ObjectLoaderRaw<FInt8Property>						load_Int8Property						("Int8Property");
ObjectLoaderRaw<FInt16Property>						load_Int16Propert						("Int16Property");
ObjectLoaderRaw<FIntProperty>						load_IntProperty						("IntProperty");
ObjectLoaderRaw<FInt64Property>						load_Int64Property						("Int64Property");
ObjectLoaderRaw<FByteProperty>						load_ByteProperty						("ByteProperty");
ObjectLoaderRaw<FUInt16Property>					load_UInt16Property						("UInt16Property");
ObjectLoaderRaw<FUInt32Property>					load_UInt32Property						("UInt32Property");
ObjectLoaderRaw<FUInt64Property>					load_UInt64Property						("UInt64Property");
ObjectLoaderRaw<FFloatProperty>						load_FloatProperty						("FloatProperty");
ObjectLoaderRaw<FDoubleProperty>					load_DoubleProperty						("DoubleProperty");
ObjectLoaderRaw<FArrayProperty>						load_ArrayProperty						("ArrayProperty");
ObjectLoaderRaw<FSetProperty>						load_SetProperty						("SetProperty");
ObjectLoaderRaw<FMapProperty>						load_MapProperty						("MapProperty");
ObjectLoaderRaw<FMulticastSparseDelegateProperty>	load_MulticastSparseDelegateProperty	("MulticastSparseDelegateProperty");
ObjectLoaderRaw<FMulticastInlineDelegateProperty>	load_MulticastInlineDelegateProperty	("MulticastInlineDelegateProperty");
ObjectLoaderRaw<FSoftObjectProperty>				load_SoftObjectProperty					("SoftObjectProperty");
ObjectLoaderRaw<FDelegateProperty>					load_DelegateProperty					("DelegateProperty");
ObjectLoaderRaw<FInterfaceProperty>					load_InterfaceProperty					("InterfaceProperty");
ObjectLoaderRaw<FFieldPathProperty>					load_FieldPathProperty					("FieldPathProperty");

template<typename T> struct TPerPlatformProperty {
	T				Default;
	TMap<FName2, T>	PerPlatform;

	bool	read(istream_linker& file) {
		bool bCooked = file.get<bool32>();
		file.read(Default);
		return bCooked || file.read(PerPlatform);
	}
};

template<typename T> ISO_DEFUSERCOMPVT(TPerPlatformProperty, T, Default, PerPlatform);
ObjectLoaderRaw<TPerPlatformProperty<int>>		load_PerPlatformInt("PerPlatformInt");
ObjectLoaderRaw<TPerPlatformProperty<float>>	load_PerPlatformFloat("PerPlatformFloat");
ObjectLoaderRaw<TPerPlatformProperty<bool>>		load_PerPlatformBool("PerPlatformBool");

//-----------------------------------------------------------------------------
//	objects
//-----------------------------------------------------------------------------

typedef UObject UMaterialInterface, UActorComponent, AActor;

bool UObject::read(istream_linker& file) {
	ReadTagged(file, *this);
	bool32	has_guid;
	if (file.read(has_guid) && has_guid)
		Append(ISO::MakePtr("guid", file.get<FGuid>()));
	return true;
}
ISO_DEFUSER(UObject, anything);
ObjectLoaderRaw<UObject>	load_Object("UObject");

bool UStruct::read(istream_linker& file) {
	UObject::read(file);
	file.read(Super, Children, ChildProperties);

	auto BytecodeBufferSize			= file.get<int32>();
	auto SerializedScriptSize		= file.get<int32>();
	Script.read(file, SerializedScriptSize);
	return true;
}

ISO_DEFUSERCOMPBV(UStruct, UObject, Super, Children, ChildProperties, Script);
ObjectLoaderRaw<UStruct>	load_Struct("Struct");

bool FImplementedInterface::read(istream_linker& file) {
	return file.read(Class, PointerOffset, bImplementedByK2);
}

bool UClass::read(istream_linker& file) {
	UStruct::read(file);
	file.read(FuncMap, ClassFlags, ClassWithin, ClassConfigName, ClassGeneratedBy, Interfaces);
	file.get<bool32>();//bool bDeprecatedForceScriptOrder = false;
	file.get<FName>();
	if (file.get<bool32>())
		file.read(CDO);
	return true;
}

ISO_DEFUSER(UClass::EClassFlags, xint32);
ISO_DEFUSERCOMPV(FImplementedInterface, Class, PointerOffset, bImplementedByK2);
ISO_DEFUSERCOMPBV(UClass, UObject, FuncMap, ClassFlags, ClassWithin, ClassGeneratedBy, ClassConfigName, Interfaces, CDO);
ObjectLoaderRaw<UClass>	load_Class("Class");
ObjectLoaderRaw<UClass>	load_BlueprintGeneratedClass("BlueprintGeneratedClass");

struct UObjectRedirector : public UObject {
	TPtr<UObject>		DestinationObject;
	bool read(istream_linker& file) {
		return UObject::read(file) && file.read(DestinationObject);
	}
};
ISO_DEFUSERCOMPBV(UObjectRedirector, UObject, DestinationObject);
ObjectLoaderRaw<UObjectRedirector>	load_ObjectRedirector("ObjectRedirector");

struct UAssetImportData : UObject {
	FString	json;
	bool read(istream_linker& file) {
		return file.read(json) && UObject::read(file);
	}
};
ISO_DEFUSERCOMPBV(UAssetImportData, UObject, json);
ObjectLoaderRaw<UAssetImportData>	load_AssetImportData("AssetImportData");
ObjectLoaderRaw<UAssetImportData>	load_FbxStaticMeshImportData("FbxStaticMeshImportData");
ObjectLoaderRaw<UAssetImportData>	load_FbxSkeletalMeshImportData("FbxSkeletalMeshImportData");

struct UMetaData : UObject {
	TMap<TPtr<UObject>, TMap<FName2, FString>>	ObjectMetaDataMap;
	TMap<FName2, FString>				RootMetaDataMap;
	bool read(istream_linker& file) {
		return UObject::read(file) && file.read(ObjectMetaDataMap) && file.read(RootMetaDataMap);
	}
};
ISO_DEFUSERCOMPBV(UMetaData, UObject, ObjectMetaDataMap, RootMetaDataMap);
ObjectLoaderRaw<UMetaData>	load_MetaData("MetaData");


enum EPixelFormat {
	PF_Unknown              =0,
	PF_A32B32G32R32F        =1,
	PF_B8G8R8A8             =2,
	PF_G8                   =3,
	PF_G16                  =4,
	PF_DXT1                 =5,
	PF_DXT3                 =6,
	PF_DXT5                 =7,
	PF_UYVY                 =8,
	PF_FloatRGB             =9,
	PF_FloatRGBA            =10,
	PF_DepthStencil         =11,
	PF_ShadowDepth          =12,
	PF_R32_FLOAT            =13,
	PF_G16R16               =14,
	PF_G16R16F              =15,
	PF_G16R16F_FILTER       =16,
	PF_G32R32F              =17,
	PF_A2B10G10R10          =18,
	PF_A16B16G16R16         =19,
	PF_D24                  =20,
	PF_R16F                 =21,
	PF_R16F_FILTER          =22,
	PF_BC5                  =23,
	PF_V8U8                 =24,
	PF_A1                   =25,
	PF_FloatR11G11B10       =26,
	PF_A8                   =27,
	PF_R32_UINT             =28,
	PF_R32_SINT             =29,
	PF_PVRTC2               =30,
	PF_PVRTC4               =31,
	PF_R16_UINT             =32,
	PF_R16_SINT             =33,
	PF_R16G16B16A16_UINT    =34,
	PF_R16G16B16A16_SINT    =35,
	PF_R5G6B5_UNORM         =36,
	PF_R8G8B8A8             =37,
	PF_A8R8G8B8				=38,	// Only used for legacy loading; do NOT use!
	PF_BC4					=39,
	PF_R8G8                 =40,	
	PF_ATC_RGB				=41,	// Unsupported Format
	PF_ATC_RGBA_E			=42,	// Unsupported Format
	PF_ATC_RGBA_I			=43,	// Unsupported Format
	PF_X24_G8				=44,	// Used for creating SRVs to alias a DepthStencil buffer to read Stencil. Don't use for creating textures
	PF_ETC1					=45,	// Unsupported Format
	PF_ETC2_RGB				=46,
	PF_ETC2_RGBA			=47,
	PF_R32G32B32A32_UINT	=48,
	PF_R16G16_UINT			=49,
	PF_ASTC_4x4             =50,	// 8.00 bpp
	PF_ASTC_6x6             =51,	// 3.56 bpp
	PF_ASTC_8x8             =52,	// 2.00 bpp
	PF_ASTC_10x10           =53,	// 1.28 bpp
	PF_ASTC_12x12           =54,	// 0.89 bpp
	PF_BC6H					=55,
	PF_BC7					=56,
	PF_R8_UINT				=57,
	PF_L8					=58,
	PF_XGXR8				=59,
	PF_R8G8B8A8_UINT		=60,
	PF_R8G8B8A8_SNORM		=61,
	PF_R16G16B16A16_UNORM	=62,
	PF_R16G16B16A16_SNORM	=63,
	PF_PLATFORM_HDR_0		=64,
	PF_PLATFORM_HDR_1		=65,	// Reserved
	PF_PLATFORM_HDR_2		=66,	// Reserved
	PF_NV12					=67,
	PF_R32G32_UINT          =68,
	PF_ETC2_R11_EAC			=69,
	PF_ETC2_RG11_EAC		=70,
	PF_MAX					=71,
};

template<typename S, typename B> void read_mips(B &&bm, istream_ref file, int mips) {
	if (mips > 1) {
		for (int i = 0; i < mips; i++) {
			auto	mip = bm->Mip(i);
			auto	src	= make_auto_block<S>(mip.template size<1>(), mip.template size<2>());
			file.readbuff(&src[0][0], num_elements_total(src) * sizeof(S));
			copy(src.get(), mip);
		}
	} else {
		auto	mip = bm->All();
		auto	src	= make_auto_block<S>(mip.template size<1>(), mip.template size<2>());
		file.readbuff(&src[0][0], num_elements_total(src) * sizeof(S));
		copy(src.get(), mip);
	}
}

ISO_ptr<void> ReadSourceBitmap(istream_ref file2, const char *Format, int SizeX, int SizeY, int NumSlices, int NumMips) {
	if (Format == "TSF_G8"_cstr || Format == "TSF_BGRA8"_cstr) {
		ISO_ptr<bitmap> bm(Format, SizeX, SizeY, NumMips > 1 ? BMF_MIPS : 0, NumSlices);
		if (Format == "TSF_G8"_cstr)
			read_mips<uint8>(bm, file2, NumMips);
		else
			read_mips<Texel<B8G8R8A8>>(bm, file2, NumMips);
		return bm;

	} else {
		ISO_ptr<HDRbitmap> bm(Format, SizeX, SizeY, NumMips > 1 ? BMF_MIPS : 0, NumSlices);
		if (Format == "TSF_G16"_cstr)
			read_mips<uint8>(bm, file2, NumMips);
		else if (Format == "TSF_BGRE8"_cstr)
			read_mips<rgbe>(bm, file2, NumMips);
		else if (Format == "TSF_RGBA16"_cstr)
			read_mips<uint16x4>(bm, file2, NumMips);
		else if (Format == "TSF_RGBA16F"_cstr)
			read_mips<hfloat4>(bm, file2, NumMips);
		return bm;
	}
}

struct MipSource {
	bool32		cooked;
	FBulkData	bulk;
	int32		SizeX, SizeY, SizeZ;
	bool read(istream_ref file) {
		return file.read(cooked, bulk, SizeX, SizeY, SizeZ);
	}
};

//template<typename S, typename B> void read_mips(B &&bm, istream_linker& file, const temp_array<MipSource> &mips) {
//	for (auto &m : mips) {
//		int		i		= mips.index_of(m);
//		copy(make_block((S*)malloc_block::unterminated(m.bulk.reader(
//			m.bulk.seperate_file() ? file.bulk_file : file,
//			file.bulk_data())), m.SizeX, m.SizeY), bm->Mip(i));
//	}
//}

template<typename S, int X=1, int Y=1,typename B> void read_mips(B &&bm, istream_linker& file, const temp_array<MipSource> &mips) {
	for (auto &m : mips) {
		int		i		= mips.index_of(m);
		copy(make_block((S*)malloc_block::unterminated(m.bulk.reader(
			m.bulk.seperate_file() ? file.bulk_file : file,
			file.bulk_data())), m.SizeX / X, m.SizeY / Y), bm->Mip(i));
	}
}

ISO_ptr<void> ReadBakedBitmap(istream_linker& file, const char *Format, const temp_array<MipSource> &mips) {
	ISO_ptr<bitmap> bm(Format, mips[0].SizeX, mips[0].SizeY, BMF_MIPS, mips[0].SizeZ);
	switch (string_hash(Format)) {
		case "PF_B8G8R8A8"_hash:	read_mips<Texel<B8G8R8A8>>(bm, file, mips); return bm;
		case "PF_G8"_hash:			read_mips<uint8>(bm, file, mips); return bm;
		case "PF_DXT1"_hash:		read_mips<const BC<1>,  4, 4>(bm, file, mips); return bm;
		case "PF_DXT3"_hash:		read_mips<const BC<2>,  4, 4>(bm, file, mips); return bm;
		case "PF_DXT5"_hash:		read_mips<const BC<3>,  4, 4>(bm, file, mips); return bm;
		case "PF_BC4"_hash:			read_mips<const BC<4>,  4, 4>(bm, file, mips); return bm;
		case "PF_BC5"_hash:			read_mips<const BC<5>,  4, 4>(bm, file, mips); return bm;
		case "PF_BC7"_hash:			read_mips<const BC<7>,  4, 4>(bm, file, mips); return bm;
		case "PF_ASTC_4x4"_hash:	read_mips<const ASTCT< 4, 4>, 4, 4>(bm, file, mips); return bm;
		case "PF_ASTC_6x6"_hash:	read_mips<const ASTCT< 6, 6>, 6, 6>(bm, file, mips); return bm;
		case "PF_ASTC_8x8"_hash:	read_mips<const ASTCT< 8, 8>, 8, 8>(bm, file, mips); return bm;
		case "PF_ASTC_10x10"_hash:	read_mips<const ASTCT<10,10>,10,10>(bm, file, mips); return bm;
		case "PF_ASTC_12x12"_hash:	read_mips<const ASTCT<12,12>,12,12>(bm, file, mips); return bm;

		case "PF_Unknown"_hash:
		case "PF_UYVY"_hash:
		case "PF_V8U8"_hash:
		case "PF_A1"_hash:			read_mips<Texel<A1>>(bm, file, mips); return bm;
		case "PF_A8"_hash:			read_mips<Texel<A8>>(bm, file, mips); return bm;
		case "PF_PVRTC2"_hash:
		case "PF_PVRTC4"_hash:
		case "PF_R8G8B8A8"_hash:	read_mips<Texel<R8G8B8A8>>(bm, file, mips); return bm;
		case "PF_A8R8G8B8"_hash:	read_mips<Texel<A8R8G8B8>>(bm, file, mips); return bm;
		case "PF_R8G8"_hash:		read_mips<Texel<R8G8>>(bm, file, mips); return bm;
		case "PF_ATC_RGB"_hash:		read_mips<const ATTrec, 4, 4>(bm, file, mips); return bm;
		case "PF_ATC_RGBA_E"_hash:	//read_mips<const ATTArec, 4, 4>(bm, file, mips); return bm;
		case "PF_ATC_RGBA_I"_hash:
		case "PF_X24_G8"_hash:
		case "PF_ETC1"_hash:		read_mips<const ETC1,4,4>(bm, file, mips); return bm;
		case "PF_ETC2_RGB"_hash:	read_mips<const ETC2_RGB,4,4>(bm, file, mips); return bm;
		case "PF_ETC2_RGBA"_hash:	read_mips<const ETC2_RGBA,4,4>(bm, file, mips); return bm;
		case "PF_R8_UINT"_hash:
		case "PF_L8"_hash:
		case "PF_XGXR8"_hash:
		case "PF_R8G8B8A8_UINT"_hash:
		case "PF_R8G8B8A8_SNORM"_hash:
		case "PF_R5G6B5_UNORM"_hash:
		case "PF_NV12"_hash:
			return ISO_NULL;
		default: {
			ISO_ptr<HDRbitmap> bm(Format, mips[0].SizeX, mips[0].SizeY, BMF_MIPS, mips[0].SizeZ);
			switch (string_hash(Format)) {
				case "PF_BC6H"_hash:			read_mips<const BC<-6>>(bm, file, mips); return bm;
				case "PF_G16"_hash:				read_mips<Texel<TexelFormat<16,0,0,0,16,0,0>>>(bm, file, mips); return bm;
				case "PF_A32B32G32R32F"_hash:
				case "PF_FloatRGB"_hash:		read_mips<float3>(bm, file, mips); return bm;
				case "PF_FloatRGBA"_hash:		read_mips<float4>(bm, file, mips); return bm;
				case "PF_DepthStencil"_hash:
				case "PF_ShadowDepth"_hash:
				case "PF_R32_FLOAT"_hash:		read_mips<float>(bm, file, mips); return bm;
				case "PF_G16R16"_hash:			read_mips<Texel<TexelFormat<32,16,16,0,16,0,0>>>(bm, file, mips); return bm;
				case "PF_G16R16F"_hash:
				case "PF_G16R16F_FILTER"_hash:
				case "PF_G32R32F"_hash:
				case "PF_A2B10G10R10"_hash:		read_mips<Texel<TexelFormat<32,22,10,12,10,2,10,0,2>>>(bm, file, mips); return bm;
				case "PF_A16B16G16R16"_hash:	read_mips<Texel<TexelFormat<64,48,16,32,16,16,16,0,16>>>(bm, file, mips); return bm;
				case "PF_D24"_hash:
				case "PF_R16F"_hash:
				case "PF_R16F_FILTER"_hash:
				case "PF_FloatR11G11B10"_hash:
				case "PF_R32_UINT"_hash:
				case "PF_R32_SINT"_hash:
				case "PF_R16_UINT"_hash:
				case "PF_R16_SINT"_hash:
				case "PF_R16G16B16A16_UINT"_hash:
				case "PF_R16G16B16A16_SINT"_hash:
				case "PF_R32G32B32A32_UINT"_hash:
				case "PF_R16G16_UINT"_hash:
				case "PF_R16G16B16A16_UNORM"_hash:
				case "PF_R16G16B16A16_SNORM"_hash:
				case "PF_PLATFORM_HDR_0"_hash:
				case "PF_PLATFORM_HDR_1"_hash:
				case "PF_PLATFORM_HDR_2"_hash:
				case "PF_R32G32_UINT"_hash:
				case "PF_ETC2_R11_EAC"_hash:	read_mips<const ETC2_R11,4,4>(bm, file, mips); return bm;
				case "PF_ETC2_RG11_EAC"_hash:	read_mips<const ETC2_RG11,4,4>(bm, file, mips); return bm;
				default: return ISO_NULL;
			}
		}
	}
}

struct UTexture2D : UObject {
	ISO_ptr<void>	Bitmap;
	bool read(istream_linker& file) {
		UObject::read(file);

		auto			strip0	= file.get<FStripDataFlags>();
		malloc_block	data;

		if (!strip0.IsEditorDataStripped()) {
			if (file.linker->CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::VirtualizedBulkDataHaveUniqueGuids) {
				if (file.linker->CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::TextureSourceVirtualization) {
					FBulkData	bulk;
					file.read(bulk);
					data = bulk.data(file, file.bulk_data());
				} else {
					//bulk.Serialize(Ar, this, false);
					//bulk.CreateLegacyUniqueIdentifier(this);
				}

			} else {
				FEditorBulkData	bulk;
				file.read(bulk);
				data = bulk.data(file);
			}
		}

		auto	strip	= file.get<FStripDataFlags>();
		auto	cooked	= file.get<bool32>();

		auto	b	= ISO::MakeBrowser(*(anything*)this);

		if (auto source = b["Source"]) {
			memory_reader	reader(data);

			if (source["bPNGCompressed"].Get<bool>()) {
				Bitmap = (make_save_pos(file), FileHandler::Get("png")->Read(none, reader));
			} else {
				Bitmap = ReadSourceBitmap(reader, source["Format"].GetString(), source["SizeX"].GetInt(), source["SizeY"].GetInt(), source["NumSlices"].GetInt(), source["NumMips"].GetInt());
			}
		} else {
			FName	PixelFormatName;
			while (file.read(PixelFormatName) && file.lookup(PixelFormatName) != "None") {
				auto	skip		= make_skip_size(file, file.get<int64>());

				int32	SizeX, SizeY;
				uint32	PackedData;
				FString PixelFormatString;
				int32	FirstMipToSerialize, NumMips;

				file.read(SizeX, SizeY, PackedData, PixelFormatString, FirstMipToSerialize, NumMips);
				temp_array<MipSource>	mips(file, NumMips);
				Bitmap = ReadBakedBitmap(file, get(PixelFormatString), mips);
			}
		}
		return true;
	}
};

typedef UTexture2D ULightMapTexture2D, UShadowMapTexture2D, ULightMapVirtualTexture2D;

ISO_DEFUSERCOMPBV(UTexture2D, UObject, Bitmap);
ObjectLoaderRaw<UTexture2D> load_Texture2D("Texture2D"),
							load_LightMapTexture2D("LightMapTexture2D"),
							load_ShadowMapTexture2D("ShadowMapTexture2D"),
							load_LightMapVirtualTexture2D("LightMapVirtualTexture2D");


//-----------------------------------------------------------------------------
//	UWorld
//-----------------------------------------------------------------------------

struct UWorld : UObject {
	TPtr<UObject>			PersistentLevel;
	TArray<TPtr<UObject>>	ExtraReferencedObjects;
	TArray<TPtr<UObject>>	StreamingLevels;

	bool	read(istream_linker& file) {
		return UObject::read(file) && file.read(PersistentLevel, ExtraReferencedObjects, StreamingLevels);
	}
};
ISO_DEFUSERCOMPBV(UWorld, UObject, PersistentLevel, ExtraReferencedObjects, StreamingLevels);
ObjectLoaderRaw<UWorld>	load_World("World");

//-----------------------------------------------------------------------------
//	physics
//-----------------------------------------------------------------------------

struct UBodySetup : UObject {
	FGuid	BodySetupGuid;
	bool	bHasCookedCollisionData;

	bool read(istream_linker& file) {
		UObject::read(file);
		file.read(BodySetupGuid);

		if (bool bCooked = file.get<bool32>()) {
			bHasCookedCollisionData = file.get<bool32>();
		}
		return true;
	}
};

ISO_DEFUSERCOMPBV(UBodySetup, UObject, BodySetupGuid, bHasCookedCollisionData);
ObjectLoaderRaw<UBodySetup>		load_BodySetup("BodySetup");
ObjectLoaderRaw<UBodySetup>		load_SkeletalBodySetup("SkeletalBodySetup");

//struct USkeletalBodySetup : UBodySetup {
//};

//-----------------------------------------------------------------------------
//	navigation
//-----------------------------------------------------------------------------

struct UNavCollision : UObject {
	enum {
		VerInitial = 1,
		VerAreaClass = 2,
		VerConvexTransforms = 3,
		VerShapeGeoExport = 4,
		VerLatest = VerShapeGeoExport,
		MagicNum = 0xA237F237,
	};
	TPtr<UClass>	AreaClass;

	bool read(istream_linker& file) {
		UObject::read(file);

		int32 Version = VerLatest;
		if (file.get<int32>() == MagicNum) {
			file.read(Version);
		} else {
			Version = VerInitial;
			file.seek_cur(-4);
		}

		auto	Guid	= file.get<FGuid>();
		bool	bCooked	= file.get<bool32>();


		if (Version >= VerAreaClass)
			file.read(AreaClass);

		return true;
	}
};

ISO_DEFUSERCOMPBV(UNavCollision, UObject, AreaClass);
ObjectLoaderRaw<UNavCollision>		load_NavCollision("NavCollision");


//-----------------------------------------------------------------------------
//	detour nav
//-----------------------------------------------------------------------------

typedef uint64 dtPolyRef;
typedef uint64 dtTileRef;
typedef uint64 dtClusterRef;

static const uint16 DT_EXT_LINK			= 0x8000;
static const uint32 DT_NULL_LINK		= 0xffffffff;

static const int	DT_MIN_SALT_BITS	= 5;
static const int	DT_SALT_BASE		= 1;

static const int	DT_MAX_OFFMESH_SEGMENT_PARTS		= 4;
static const int	DT_INVALID_SEGMENT_PART				= 0xffff;

static const uint8	DT_CONNECTION_INTERNAL				= (1 << 7);
static const uint8	DT_LINK_FLAG_OFFMESH_CON			= (1 << 6);
static const uint8	DT_LINK_FLAG_OFFMESH_CON_BIDIR		= (1 << 5);
static const uint8	DT_LINK_FLAG_OFFMESH_CON_BACKTRACKER= (1 << 4);
static const uint8	DT_LINK_FLAG_OFFMESH_CON_ENABLED	= (1 << 3);
static const uint8	DT_LINK_FLAG_SIDE_MASK				= 7;

static const uint8	DT_CLINK_VALID_FWD					= 0x01;
static const uint8	DT_CLINK_VALID_BCK					= 0x02;

static const uint32 DT_CLINK_FIRST						= 0x80000000;

enum dtTileFlags {	
	DT_TILE_FREE_DATA = 0x01,	// The navigation mesh owns the tile memory and is responsible for freeing it
};

/// Flags representing the type of a navigation mesh polygon
enum dtPolyTypes {
	DT_POLYTYPE_GROUND			= 0,
	DT_POLYTYPE_OFFMESH_POINT	= 1,
	DT_POLYTYPE_OFFMESH_SEGMENT	= 2,
};

struct dtPoly {
	uint32	firstLink;
	uint16	verts[6];
	uint16	neis[6];
	uint16	flags;
	uint8	vertCount;
	uint8	areaAndtype;
};

struct dtPolyDetail {
	uint32	vertBase;
	uint32	triBase;
	uint8	vertCount;
	uint8	triCount;
	bool	read(istream_ref file) {
		return file.read(vertBase, triBase, vertCount, triCount);
	}
};

struct dtLink {
	dtPolyRef	ref;
	uint32		next;
	uint8		edge;
	uint8		side;
	uint8		bmin;
	uint8		bmax;
};

struct dtBVNode {
	uint16	bmin[3];
	uint16	bmax[3];
	int		i;
};

struct dtOffMeshSegmentConnection {
	float	startA[3];
	float	endA[3];
	float	startB[3];
	float	endB[3];
	float	rad;
	float	height;
	uint32	userId;
	uint16	firstPoly;
	uint8	npolys;
	uint8	flags;
	bool	read(istream_ref file) {
		return file.read(startA, startB, endA, endB, rad, firstPoly, npolys, flags, userId);
	}
};

struct dtOffMeshConnection {
	enum {
		BIDIR		= 0x01,
		POINT		= 0x02,
		SEGMENT		= 0x04,
		CHEAPAREA	= 0x08,
	};
	float	pos[6];
	float	rad;
	float	height;
	uint16	poly;
	uint8	flags;
	uint8	side;
	uint32	userId;
	bool	read(istream_ref file) {
		return file.read(pos, rad, poly, flags, side, userId);
	}
};

struct dtCluster {
	float center[3];				///< Center pos of cluster
};

struct dtClusterLink {
	dtClusterRef	ref;
	uint32			next;
	uint8			flags;
};

struct dtMeshHeader {
	enum {
		MAGIC			= 'D'<<24 | 'N'<<16 | 'A'<<8 | 'V',
		VERSION			= 7,
		STATE_MAGIC		= 'D'<<24 | 'N'<<16 | 'M'<<8 | 'S',
		STATE_VERSION	= 1,
	};

	int		magic;				///< Tile magic number. (Used to identify the data format.)
	int		version;			///< Tile data format version number
	int		x;					///< The x-position of the tile within the dtNavMesh tile grid. (x, y, layer)
	int		y;					///< The y-position of the tile within the dtNavMesh tile grid. (x, y, layer)
	int		layer;				///< The layer of the tile within the dtNavMesh tile grid. (x, y, layer)
	uint32	userId;				///< The user defined id of the tile
	int		polyCount;			///< The number of polygons in the tile
	int		vertCount;			///< The number of vertices in the tile
	int		maxLinkCount;		///< The number of allocated links
	int		detailMeshCount;	///< The number of sub-meshes in the detail mesh
	int		detailVertCount;	/// The number of unique vertices in the detail mesh. (In addition to the polygon vertices.)
	int		detailTriCount;		///< The number of triangles in the detail mesh
	int		bvNodeCount;		///< The number of bounding volume nodes. (Zero if bounding volumes are disabled.)
	int		offMeshConCount;	///< The number of point type off-mesh connections
	int		offMeshBase;		///< The index of the first polygon which is an point type off-mesh connection
	float	walkableHeight;		///< The height of the agents using the tile
	float	walkableRadius;		///< The radius of the agents using the tile
	float	walkableClimb;		///< The maximum climb height of the agents using the tile
	float	bmin[3];			///< The minimum bounds of the tile's AABB. [(x, y, z)]
	float	bmax[3];			///< The maximum bounds of the tile's AABB. [(x, y, z)]
	float	bvQuantFactor;		/// The bounding volume quantization factor
	int		clusterCount;		///< Number of clusters
	int		offMeshSegConCount;	///< The number of segment type off-mesh connections
	int		offMeshSegPolyBase;	///< The index of the first polygon which is an segment type off-mesh connection
	int		offMeshSegVertBase;	///< The index of the first vertex used by segment type off-mesh connection
};
#if 0
struct dtMeshTile {
	uint32 salt;					///< Counter describing modifications to the tile
	uint32 linksFreeList;			///< Index to the next free link
	dtMeshHeader* header;				///< The tile header
	dtPoly* polys;						///< The tile polygons. [Size: dtMeshHeader::polyCount]
	float* verts;						///< The tile vertices. [Size: dtMeshHeader::vertCount]
	dtLink* links;						///< The tile links. [Size: dtMeshHeader::maxLinkCount]
	dtPolyDetail* detailMeshes;			///< The tile's detail sub-meshes. [Size: dtMeshHeader::detailMeshCount]
	float* detailVerts;					/// The detail mesh's unique vertices. [(x, y, z) * dtMeshHeader::detailVertCount]

	uint8* detailTris;	
	dtBVNode* bvTree;
	dtOffMeshConnection* offMeshCons;		///< The tile off-mesh connections. [Size: dtMeshHeader::offMeshConCount]
	dtOffMeshSegmentConnection* offMeshSeg;	///< The tile off-mesh connections. [Size: dtMeshHeader::offMeshSegConCount]

	uint8* data;					///< The tile data. (Not directly accessed under normal situations.)
	int dataSize;							///< Size of the tile data
	int flags;								///< Tile flags. (See: #dtTileFlags)
	dtMeshTile* next;						///< The next free tile, or the next tile in the spatial grid
	dtCluster* clusters;					///< Cluster data
	uint16* polyClusters;			///< Cluster Id for each ground type polygon [Size: dtMeshHeader::polyCount]
	dtChunkArray<dtLink> dynamicLinksO;			///< Dynamic links array (indices starting from dtMeshHeader::maxLinkCount)
	uint32 dynamicFreeListO;				///< Index of the next free dynamic link
	dtChunkArray<dtClusterLink> dynamicLinksC;	///< Dynamic links array (indices starting from DT_CLINK_FIRST)
	uint32 dynamicFreeListC;				///< Index of the next free dynamic link
};
struct dtNavMeshParams {
	float orig[3];					///< The world space origin of the navigation mesh's tile space. [(x, y, z)]
	float tileWidth;				///< The width of each tile. (Along the x-axis.)
	float tileHeight;				///< The height of each tile. (Along the z-axis.)
	int maxTiles;					///< The maximum number of tiles the navigation mesh can contain
	int maxPolys;					///< The maximum number of polygons each tile can contain
};

struct dtNavMesh {
	static const int DT_MAX_AREAS = 64;

	dtNavMeshParams m_params;			///< Current initialization params. TODO: do not store this info twice
	float m_orig[3];					///< Origin of the tile (0,0)
	float m_tileWidth, m_tileHeight;	///< Dimensions of each tile
	int m_maxTiles;						///< Max number of tiles
	int m_tileLutSize;					///< Tile hash lookup size (must be pot)
	int m_tileLutMask;					///< Tile hash lookup mask

	uint8 m_areaCostOrder[DT_MAX_AREAS];

	dtMeshTile** m_posLookup;			///< Tile hash lookup
	dtMeshTile* m_nextFree;				///< Freelist of tiles
	dtMeshTile* m_tiles;				///< List of tiles

	uint32 m_saltBits;			///< Number of salt bits in the tile ID
	uint32 m_tileBits;			///< Number of tile bits in the tile ID
	uint32 m_polyBits;			///< Number of poly bits in the tile ID
};
#endif

struct FTileRawData {
	dtMeshHeader							header;
	dynamic_array<FVector>					nav_verts;
	dynamic_array<dtPoly>					nav_polys;
	dynamic_array<dtPolyDetail>				detail_meshes;
	dynamic_array<FVector>					detail_verts;
	dynamic_array<array<uint8, 4>>	detail_tris;
	dynamic_array<dtBVNode>					bv_tree;
	dynamic_array<dtOffMeshConnection>		off_mesh_cons;
	dynamic_array<dtOffMeshConnection>		off_mesh_segs;
	dynamic_array<dtCluster>				clusters;
	dynamic_array<uint16>					poly_clusters;

	bool	read(istream_ref file) {
		struct Header0 {
			int32 totVertCount;
			int32 totPolyCount;
			int32 maxLinkCount;
			int32 detailMeshCount;
			int32 detailVertCount;
			int32 detailTriCount;
			int32 bvNodeCount;
			int32 offMeshConCount;
			int32 offMeshSegConCount;
			int32 clusterCount;
		};

		auto header0 = file.get<Header0>();
		int32 polyClusterCount = header0.detailMeshCount;

		file.read(header);
		nav_verts.read(file, header0.totVertCount);
		nav_polys.read(file, header0.totPolyCount);
		detail_meshes.read(file, header0.detailMeshCount);
		detail_verts.read(file, header0.detailVertCount);
		detail_tris.read(file, header0.detailTriCount);
		bv_tree.read(file, header0.bvNodeCount);
		off_mesh_cons.read(file, header0.offMeshConCount);

			for (auto &i : off_mesh_cons) {
				file.read(i.height);
			}

		off_mesh_segs.read(file, header0.offMeshSegConCount);
		clusters.read(file, header0.clusterCount);
		poly_clusters.read(file, polyClusterCount);
		return true;
	}
};

ISO_DEFUSERCOMPV(dtPoly, firstLink, verts, neis, flags, vertCount, areaAndtype);
ISO_DEFUSERCOMPV(dtPolyDetail, vertBase, triBase, vertCount, triCount);
ISO_DEFUSERCOMPV(dtLink, ref, next, edge, side, bmin, bmax);
ISO_DEFUSERCOMPV(dtBVNode, bmin, bmax, i);
ISO_DEFUSERCOMPV(dtOffMeshSegmentConnection, startA, endA, startB, endB, rad, height, userId, firstPoly, npolys, flags);
ISO_DEFUSERCOMPV(dtOffMeshConnection, pos, rad, height, poly, flags, side, userId);
ISO_DEFUSERCOMPV(dtCluster, center);
ISO_DEFUSERCOMPV(dtClusterLink, ref, next, flags);
ISO_DEFUSERCOMPV(dtMeshHeader, magic, version, x, y, layer, userId, polyCount, vertCount, maxLinkCount, detailMeshCount, detailVertCount, detailTriCount, bvNodeCount, offMeshConCount, offMeshBase, walkableHeight, walkableRadius, walkableClimb, bmin, bmax, bvQuantFactor, clusterCount, offMeshSegConCount, offMeshSegPolyBase, offMeshSegVertBase);
ISO_DEFUSERCOMPV(FTileRawData, header, nav_verts, nav_polys, detail_meshes, detail_verts, detail_tris, bv_tree, off_mesh_cons, off_mesh_segs, clusters, poly_clusters);

struct FRecastTileData {
	TKnownPtr<FTileRawData>	TileRawData;
	malloc_block			TileCacheRawData;

	bool read(istream_linker& file) {
		auto	TileDataSize	= file.get<int32>();
		TileRawData.read(file);
		TileCacheRawData.read(file, file.get<int32>());
		return true;
	}
};

struct URecastNavMeshDataChunk : UObject {
	TArray<FRecastTileData> Tiles;

	bool read(istream_linker& file) {
		UObject::read(file);
		auto	ver		= file.get<int32>();
		auto	size	= file.get<int64>();
		if (size > 4)
			Tiles.read(file);
		return true;
	}
};

ISO_DEFUSERCOMPV(FRecastTileData, TileRawData, TileCacheRawData);
ISO_DEFUSERCOMPBV(URecastNavMeshDataChunk, UObject, Tiles);
ObjectLoaderRaw<URecastNavMeshDataChunk>		load_RecastNavMeshDataChunk("RecastNavMeshDataChunk");

struct FSpeedTreeWind {
	enum Constants {
		NUM_WIND_POINTS_IN_CURVE = 10,
		NUM_BRANCH_LEVELS = 2,
		NUM_LEAF_GROUPS = 2
	};
	enum EOptions {
		GLOBAL_WIND,					GLOBAL_PRESERVE_SHAPE,
		BRANCH_SIMPLE_1,				BRANCH_DIRECTIONAL_1,		BRANCH_DIRECTIONAL_FROND_1,		BRANCH_TURBULENCE_1,		BRANCH_WHIP_1,		BRANCH_OSC_COMPLEX_1,
		BRANCH_SIMPLE_2,				BRANCH_DIRECTIONAL_2,		BRANCH_DIRECTIONAL_FROND_2,		BRANCH_TURBULENCE_2,		BRANCH_WHIP_2,		BRANCH_OSC_COMPLEX_2,
		LEAF_RIPPLE_VERTEX_NORMAL_1,	LEAF_RIPPLE_COMPUTED_1,		LEAF_TUMBLE_1,					LEAF_TWITCH_1,				LEAF_OCCLUSION_1,
		LEAF_RIPPLE_VERTEX_NORMAL_2,	LEAF_RIPPLE_COMPUTED_2,		LEAF_TUMBLE_2,					LEAF_TWITCH_2,				LEAF_OCCLUSION_2,
		FROND_RIPPLE_ONE_SIDED,			FROND_RIPPLE_TWO_SIDED,		FROND_RIPPLE_ADJUST_LIGHTING,
		ROLLING,
		NUM_WIND_OPTIONS
	};

	enum EOscillationComponents {
		OSC_GLOBAL, 
		OSC_BRANCH_1, 
		OSC_BRANCH_2, 
		OSC_LEAF_1_RIPPLE, 
		OSC_LEAF_1_TUMBLE, 
		OSC_LEAF_1_TWITCH, 
		OSC_LEAF_2_RIPPLE, 
		OSC_LEAF_2_TUMBLE, 
		OSC_LEAF_2_TWITCH, 
		OSC_FROND_RIPPLE, 
		NUM_OSC_COMPONENTS
	};

	struct Params {
		float		StrengthResponse, DirectionResponse;
		float		AnchorOffset, AnchorDistanceScale;

		float		Frequencies[NUM_OSC_COMPONENTS][NUM_WIND_POINTS_IN_CURVE];

		struct {
			float	Height, HeightExponent;
			float	Distance[NUM_WIND_POINTS_IN_CURVE];
			float	DirectionAdherence[NUM_WIND_POINTS_IN_CURVE];
		} Global;

		struct {
			float	Distance[NUM_WIND_POINTS_IN_CURVE];
			float	DirectionAdherence[NUM_WIND_POINTS_IN_CURVE];
			float	Whip[NUM_WIND_POINTS_IN_CURVE];
			float	Turbulence, Twitch, TwitchFreqScale;
		} Branch[NUM_BRANCH_LEVELS];

		struct {
			float		RippleDistance[NUM_WIND_POINTS_IN_CURVE];
			struct {
				float	Flip[NUM_WIND_POINTS_IN_CURVE];
				float	Twist[NUM_WIND_POINTS_IN_CURVE];
				float	DirectionAdherence[NUM_WIND_POINTS_IN_CURVE];
			}  Tumble;
			struct {
				float	Throw[NUM_WIND_POINTS_IN_CURVE];
				float	Sharpness;
			} Twitch;
			struct {
				float	MaxScale, MinScale, Speed, Separation;
			} Roll;
			float		LeewardScalar;
		} Leaf[NUM_LEAF_GROUPS];

		struct {
			float	Distance[NUM_WIND_POINTS_IN_CURVE];
			float	Tile;
			float	LightingScalar;
		} FrondRipple;

		struct {
			struct {
				float	Size, Twist, Turbulence, Period, Speed;
			} Noise;
			struct {
				float	FieldMin, LightingAdjust, VerticalOffset;
			} Branch;
			struct {
				float	RippleMin, TumbleMin;
			} Leaf;
		} Rolling;

		struct {
			float	Frequency, StrengthMin, StrengthMax, DurationMin, DurationMax, RiseScalar, FallScalar;
		} Gusting;
	};
	Params		params;
	bool32		options[NUM_WIND_OPTIONS];
	FVector		BranchWindAnchor;
	float		MaxBranchLevel1Length;

	bool	read(istream_ref file) {
		return file.read(params, options, BranchWindAnchor, MaxBranchLevel1Length);
	}
};

//-----------------------------------------------------------------------------
//	mesh
//-----------------------------------------------------------------------------

struct FRawMesh {
	TArray<int32>		FaceMaterialIndices;
	TArray<uint32>		FaceSmoothingMasks;
	TArray<FVector>		VertexPositions;
	TArray<uint32>		WedgeIndices;
	TArray<FVector>		WedgeTangentX;
	TArray<FVector>		WedgeTangentY;
	TArray<FVector>		WedgeTangentZ;
	TArray<FVector2D>	WedgeTexCoords[8];
	TArray<FColor>		WedgeColors;
	TArray<int32>		MaterialIndexToImportIndex;

	enum {
		RAW_MESH_VER_INITIAL = 0,
		RAW_MESH_VER_REMOVE_ZERO_TRIANGLE_SECTIONS,

		RAW_MESH_VER_PLUS_ONE,
		RAW_MESH_VER = RAW_MESH_VER_PLUS_ONE - 1,

		RAW_MESH_LIC_VER_INITIAL = 0,

		RAW_MESH_LIC_VER_PLUS_ONE,
		RAW_MESH_LIC_VER = RAW_MESH_LIC_VER_PLUS_ONE - 1
	};

	bool	read(istream_ref file) {
		int32 Version			= RAW_MESH_VER;
		int32 LicenseeVersion	= RAW_MESH_LIC_VER;
		return file.read(Version, LicenseeVersion,
			FaceMaterialIndices, FaceSmoothingMasks, VertexPositions, WedgeIndices, WedgeTangentX,WedgeTangentY,WedgeTangentZ,
			WedgeTexCoords, WedgeColors, MaterialIndexToImportIndex
		);
	}

};

template<typename T> struct AttributeValues;

struct AttributeValuesBase {
	template<typename T> auto& as() const { return *static_cast<const AttributeValues<T>*>(this); }
};

template<typename T> struct DefaultT : T_type<T> {};
template<> struct DefaultT<bool> : T_type<bool32> {};

template<typename T> struct AttributeValues : AttributeValuesBase, TArray<TBulkArray<T>> {
	typename DefaultT<T>::type	Default;
	bool read(istream_ref file) {
		TArray<TBulkArray<T>>::read(file);
		file.read(Default);
		return true;
	}
};

struct AttributeValuesDesc {
	uint32					size;
	AttributeValuesBase*	(*create)(istream_ref file);
	template<typename T> AttributeValuesDesc(T*) :
		size(sizeof32(T)),
		create([](istream_ref file)->AttributeValuesBase* { auto p = new AttributeValues<T>(); file.read(*p); return p; })
	{}
};

template<typename T> struct AttributeValuesDescs;
template<typename...T> struct AttributeValuesDescs<type_list<T...>> : array<AttributeValuesDesc, sizeof...(T)> {
	AttributeValuesDescs() : array<AttributeValuesDesc, sizeof...(T)>({AttributeValuesDesc((T*)nullptr)...}) {}
};

struct FMeshDescription {
	using AttributeTypes = type_list<
		FVector4,
		FVector,
		FVector2D,
		float,
		int,
		bool,
		FString
	>;
	static AttributeValuesDescs<AttributeTypes>	descs;

	struct Attribute {
		enum Flags : uint32 {
			None				= 0,
			Lerpable			= 1 << 0,	// Attribute can be automatically lerped according to the value of 2 or 3 other attributes
			AutoGenerated		= 1 << 1,	// Attribute is auto-generated by importer or editable mesh, rather than representing an imported property
			Mergeable			= 1 << 2,	// If all vertices' attributes are mergeable, and of near-equal value, they can be welded
			Transient			= 1 << 3	// Attribute is not serialized
		};
		struct index {
			const Attribute	*a;
			int				i;
			template<typename T> operator const T*() const {
				auto	type = meta::TL_find<T, AttributeTypes>;
				ISO_ASSERT(a->type == type);
				return a->arrays->as<T>()[i];
			}
		};

		ISO::Browser2	as_arrays() const {
			auto	p = get(arrays);
			switch (type) {
				case 0: return ISO::MakeBrowser(p->as<FVector4>());
				case 1: return ISO::MakeBrowser(p->as<FVector>());
				case 2: return ISO::MakeBrowser(p->as<FVector2D>());
				case 3: return ISO::MakeBrowser(p->as<float>());
				case 4: return ISO::MakeBrowser(p->as<int>());
				case 5: return ISO::MakeBrowser(p->as<bool>());
				case 6: return ISO::MakeBrowser(p->as<FString>());
			}
			return ISO::Browser();
		}

		uint32		type;
		unique_ptr<AttributeValuesBase>	arrays;
		uint32		num_elements;
		Flags		flags;

		Attribute()		{}
		~Attribute()	{}
		Attribute(Attribute&&) = default;
		Attribute& operator=(Attribute&&) = default;

		index	operator[](int i) const {
			return {this, i};
		}

		bool read(istream_ref file) {
			file.read(type);
			file.read(num_elements);
			arrays = descs[type].create(file);
			file.read(flags);
			return true;
		}
	};
	struct AttributesSet : TMap<FString, Attribute> {
		typedef TMap<FString, Attribute>	B;
		uint32		size;
		bool read(istream_ref file) {
			return file.read(size) && B::read(file);
		}
	};

	struct FMeshVertex {
		TArray<int> VertexInstanceIDs;
		TArray<int> ConnectedEdgeIDs;
		bool	read(istream_ref file) { return true; }
	};
	struct FMeshVertexInstance {
		int			VertexID;
		TArray<int>	ConnectedTriangles;
		bool	read(istream_ref file) { return file.read(VertexID); }
	};
	struct FMeshEdge {
		int			VertexIDs[2];
		TArray<int>	ConnectedTriangles;
		bool	read(istream_ref file) { return file.read(VertexIDs); }
	};
	struct FMeshTriangle {
		int			VertexInstanceIDs[3];
		int			PolygonID;
		bool	read(istream_ref file) { return file.read(VertexInstanceIDs, PolygonID); }
	};
	struct FMeshPolygon {
		TArray<int> VertexInstanceIDs;
		TArray<int> TriangleIDs;
		int			PolygonGroupID;
		bool	read(istream_ref file) { return file.read(VertexInstanceIDs, PolygonGroupID); }
	};
	struct FMeshPolygonGroup {
		TArray<int> Polygons;
		bool	read(istream_ref file) { return true; }
	};

	TSparseArray<FMeshVertex>			VertexArray;
	TSparseArray<FMeshVertexInstance>	VertexInstanceArray;
	TSparseArray<FMeshEdge>				EdgeArray;
	TSparseArray<FMeshTriangle>			TriangleArray;
	TSparseArray<FMeshPolygon>			PolygonArray;
	TSparseArray<FMeshPolygonGroup>		PolygonGroupArray;

	AttributesSet						VertexAttributesSet;
	AttributesSet						VertexInstanceAttributesSet;
	AttributesSet						EdgeAttributesSet;
	AttributesSet						TriangleAttributesSet;
	AttributesSet						PolygonAttributesSet;
	AttributesSet						PolygonGroupAttributesSet;

	int GetVertexPairEdge(int VertexID0, int VertexID1) const {
		for (auto i : VertexArray[VertexID0].ConnectedEdgeIDs) {
			auto EdgeVertexID0 = EdgeArray[i].VertexIDs[0];
			auto EdgeVertexID1 = EdgeArray[i].VertexIDs[1];
			if ((EdgeVertexID0 == VertexID0 && EdgeVertexID1 == VertexID1) || (EdgeVertexID0 == VertexID1 && EdgeVertexID1 == VertexID0)) {
				return i;
			}
		}
		return -1;
	}

	int GetVertexInstancePairEdge(int VertexInstanceID0, int VertexInstanceID1) const {
		return GetVertexPairEdge(VertexInstanceArray[VertexInstanceID0].VertexID, VertexInstanceArray[VertexInstanceID1].VertexID);
	}

	bool	read(istream_ref file);
	void	InitSubmesh(SubMesh* sm) const;
};

AttributeValuesDescs<FMeshDescription::AttributeTypes>	FMeshDescription::descs;

bool	FMeshDescription::read(istream_ref file) {
	if (!file.read(
		VertexArray,			VertexInstanceArray,			EdgeArray,			PolygonArray,			PolygonGroupArray,
		VertexAttributesSet,	VertexInstanceAttributesSet,	EdgeAttributesSet,	PolygonAttributesSet,	PolygonGroupAttributesSet,
		TriangleArray,			TriangleAttributesSet
	))
		return false;

	for (auto &i : VertexInstanceArray)
		VertexArray[i->VertexID].VertexInstanceIDs.push_back(VertexInstanceArray.index_of(i));

	for (auto &i : EdgeArray) {
		auto	EdgeID = EdgeArray.index_of(i);
		VertexArray[i->VertexIDs[0]].ConnectedEdgeIDs.push_back(EdgeID);
		VertexArray[i->VertexIDs[1]].ConnectedEdgeIDs.push_back(EdgeID);
	}

	for (auto &i : TriangleArray)
		PolygonArray[i->PolygonID].TriangleIDs.push_back(TriangleArray.index_of(i));

	for (auto &i : PolygonArray) {
		if (i->VertexInstanceIDs.empty()) {
			ISO_ASSERT(i->TriangleIDs.size() == 1);
			for (auto j : TriangleArray[i->TriangleIDs[0]].VertexInstanceIDs)
				i->VertexInstanceIDs.push_back(j);
		}
		PolygonGroupArray[i->PolygonGroupID].Polygons.push_back(PolygonArray.index_of(i));
	}

	for (auto &i : TriangleArray) {
		auto	TriangleID = TriangleArray.index_of(i);
		int		j0 = i->VertexInstanceIDs[2];
		for (auto j : i->VertexInstanceIDs) {
			int EdgeID = GetVertexInstancePairEdge(j0, j);
			VertexInstanceArray[j0].ConnectedTriangles.push_back(TriangleID);
			EdgeArray[EdgeID].ConnectedTriangles.push_back(TriangleID);
			j0	= j;
		}
	}
	return true;
}

void FMeshDescription::InitSubmesh(SubMesh* sm) const {
	sm->technique	= ISO::root("data")["default"]["specular"];

	int		nt	= TriangleArray.size();
	sm->NumFaces(nt);
	auto	*di	= sm->indices.begin();
	for (auto &i : TriangleArray) {
		(*di)[0]	= i->VertexInstanceIDs[2];
		(*di)[1]	= i->VertexInstanceIDs[1];
		(*di)[2]	= i->VertexInstanceIDs[0];
		++di;
	}

	const FVector	*pos	= get(VertexAttributesSet["Position "])[0];
	const FVector	*norms	= get(VertexInstanceAttributesSet["Normal"])[0];
	int		nv		= VertexInstanceArray.size();
	auto	dv		= sm->CreateVerts<UnrealVertex>(nv);

	for (auto &i : VertexInstanceArray) {
		int	v = i->VertexID;
		dv->pos.x	= pos[v].X;
		dv->pos.y	= pos[v].Y;
		dv->pos.z	= pos[v].Z;

		dv->norm.x	= norms[i.i].X;
		dv->norm.y	= norms[i.i].Y;
		dv->norm.z	= norms[i.i].Z;
		++dv;
	}
	sm->UpdateExtent();
}

struct FRawMeshBulkData : FBulkData {
	FGuid				Guid;
	bool32				bGuidIsHash;
	bool	read(istream_ref file) {
		return FBulkData::read(file) && file.read(Guid, bGuidIsHash);
	}
};

struct FMeshUVChannelInfo {
	bool32				bInitialized;
	bool32				bOverrideDensities;
	float				LocalUVDensities[4];
	bool	read(istream_ref file) {
		return file.read(bInitialized, bOverrideDensities, LocalUVDensities);
	}
};

struct FStaticMaterial {
	TPtr<UMaterialInterface>	MaterialInterface;
	FName2						MaterialSlotName;
	FName2						ImportedMaterialSlotName;
	FMeshUVChannelInfo			UVChannelData;
 
	bool	read(istream_linker &file) {
		return file.read(MaterialInterface, MaterialSlotName, ImportedMaterialSlotName, UVChannelData);
	}
};

struct UStaticMesh : UObject {
	TPtr<UBodySetup>				BodySetup;
	TPtr<UNavCollision>				NavCollision;
	FGuid							LightingGuid;
	TArray<UObject>					Sockets;
//	TKnownPtr<FSpeedTreeWind>		SpeedTreeWind;
	TArray<FStaticMaterial>			StaticMaterials;
	dynamic_array<FMeshDescription>	MeshDescriptions;
	ISO_ptr<Model3>					Model;

	bool read(istream_linker& file) {
		UObject::read(file);

		auto	StripFlags	= file.get<FStripDataFlags>();
		auto	bCooked		= file.get<bool32>();

		file.read(BodySetup);
		file.read(NavCollision);

		if (!StripFlags.IsEditorDataStripped()) {
			file.get<FString>();
			file.get<uint32>();
		}

		file.read(LightingGuid, Sockets);

		if (!StripFlags.IsEditorDataStripped()) {
			auto	SourceModels = ISO::Browser((*this)["SourceModels"]);
			for (auto &&i : SourceModels) {
				if (file.linker->CustomVer(FEditorObjectVersion::GUID) < FEditorObjectVersion::StaticMeshDeprecatedRawMesh) {
					bool bIsValid = file.get<bool32>();
					if (bIsValid) {
						auto	bulk	= file.get<FRawMeshBulkData>();
						auto	pos		= make_save_pos(file);
						MeshDescriptions.push_back().read(bulk.reader(file, file.bulk_data()));
					}
				} else if (file.linker->CustomVer(FUE5MainStreamObjectVersion::GUID) < FUE5MainStreamObjectVersion::SerializeMeshDescriptionBase) {
					bool bIsValid = file.get<bool32>();
					if (bIsValid) {
						auto	bulk = ISO::Browser((*this)["StaticMeshDescriptionBulkData"]);
						//TObjectPtr<UStaticMeshDescriptionBulkData> StaticMeshDescriptionBulkData;
					}
				}

			}
		}

		if (MeshDescriptions) {
			Model.Create(ISO::Browser((*this)["Name"]).GetString());
			for (auto &i : MeshDescriptions) {
				ISO_ptr<SubMesh>	mesh(0);
				Model->submeshes.Append(mesh);
				i.InitSubmesh(mesh);
			}
			Model->UpdateExtents();
		}

		bool bHasSpeedTreeWind = file.get<bool32>();
		if (bHasSpeedTreeWind) {
			file.get<FSpeedTreeWind>();
			return true;
		}

		file.read(StaticMaterials);

		return true;
	}
};

template<typename T> struct ISO::def<AttributeValues<T>> : ISO::def<TArray<TBulkArray<T>>> {};

ISO_DEFUSERCOMPV(FMeshDescription::Attribute, type, num_elements, as_arrays);
ISO_DEFUSERCOMPBV(FMeshDescription::AttributesSet, FMeshDescription::AttributesSet::B, size);

ISO_DEFUSERCOMPV(FMeshDescription::FMeshVertex,VertexInstanceIDs,ConnectedEdgeIDs);
ISO_DEFUSERCOMPV(FMeshDescription::FMeshVertexInstance,VertexID,ConnectedTriangles);
ISO_DEFUSERCOMPV(FMeshDescription::FMeshEdge,VertexIDs,ConnectedTriangles);
ISO_DEFUSERCOMPV(FMeshDescription::FMeshTriangle,VertexInstanceIDs,PolygonID);
ISO_DEFUSERCOMPV(FMeshDescription::FMeshPolygon,VertexInstanceIDs,TriangleIDs,PolygonGroupID);
ISO_DEFUSERCOMPV(FMeshDescription::FMeshPolygonGroup,Polygons);

ISO_DEFUSERCOMPV(FMeshDescription,		VertexArray, VertexInstanceArray, EdgeArray,  TriangleArray, PolygonArray, PolygonGroupArray, VertexAttributesSet, VertexInstanceAttributesSet, EdgeAttributesSet, TriangleAttributesSet, PolygonAttributesSet, PolygonGroupAttributesSet);
ISO_DEFUSERCOMPV(FMeshUVChannelInfo,	bInitialized, bOverrideDensities, LocalUVDensities);
ISO_DEFUSERCOMPV(FStaticMaterial,		MaterialInterface, MaterialSlotName, ImportedMaterialSlotName, UVChannelData);
ISO_DEFUSERCOMPBV(UStaticMesh, UObject, BodySetup, NavCollision, LightingGuid, Sockets, StaticMaterials, MeshDescriptions, Model);
ObjectLoaderRaw<UStaticMesh>		load_StaticMesh("StaticMesh");

struct FColorVertexBuffer {
	TBulkArray<FColor>		VertexData;
	uint32					Stride;

	bool read(istream_ref file) {
		auto StripFlags	= file.get<FStripDataFlags>();
		file.read(Stride);
		uint32	n = file.get<uint32>();
		if (n && !StripFlags.IsDataStrippedForServer())
			file.read(VertexData);
		return true;
	}
};
struct FPaintedVertex {
	FVector		Position;
	FColor		Color;
	FVector4	Normal;
};
struct FStaticMeshComponentLODInfo {
	FGuid					MapBuildDataId;
	TArray<FPaintedVertex>	PaintedVertices;
	FColorVertexBuffer		OverrideVertexColors;

	bool	read(istream_ref file) {
		auto	strip		= file.get<FStripDataFlags>();

		if (!strip.IsDataStrippedForServer())
			file.read(MapBuildDataId);
	
		if (file.get<uint8>())
			file.read(OverrideVertexColors);

		if (!strip.IsEditorDataStripped())
			file.read(PaintedVertices);
		return true;
	}
};

struct UStaticMeshComponent : UObject {
	TArray<FStaticMeshComponentLODInfo>	LODData;
	bool	read(istream_linker& file) {
		return UObject::read(file) && file.read(LODData);
	}
};

ISO_DEFUSERCOMPV(FColorVertexBuffer, VertexData, Stride);
ISO_DEFUSERCOMPV(FPaintedVertex, Position, Color, Normal);
ISO_DEFUSERCOMPV(FStaticMeshComponentLODInfo, MapBuildDataId, PaintedVertices, OverrideVertexColors);
ISO_DEFUSERCOMPBV(UStaticMeshComponent, UObject, LODData);

ObjectLoaderRaw<UStaticMeshComponent>	load_StaticMeshComponent("StaticMeshComponent");

//-----------------------------------------------------------------------------
//	HISM
//-----------------------------------------------------------------------------

struct UInstancedStaticMeshComponent : UStaticMeshComponent {
	TBulkArray<FMatrix>	PerInstanceSMData;
	TBulkArray<float>	PerInstanceSMCustomData;

	bool	read(istream_linker& file) {
		UStaticMeshComponent::read(file);
		bool	bCooked = file.get<bool32>();
		return file.read(PerInstanceSMData, PerInstanceSMCustomData);
	}
};
ISO_DEFUSERCOMPBV(UInstancedStaticMeshComponent, UStaticMeshComponent, PerInstanceSMData, PerInstanceSMCustomData);
ObjectLoaderRaw<UInstancedStaticMeshComponent>	load_InstancedStaticMeshComponent("InstancedStaticMeshComponent");

struct FClusterNode {
	FVector	BoundMin;
	int32	FirstChild;
	FVector	BoundMax;
	int32	LastChild;
	int32	FirstInstance;
	int32	LastInstance;
	FVector MinInstanceScale;
	FVector MaxInstanceScale;
};

struct UHierarchicalInstancedStaticMeshComponent : UInstancedStaticMeshComponent {
	TBulkArray<FClusterNode>	Nodes;
	bool	read(istream_linker& file) {
		return UInstancedStaticMeshComponent::read(file) && file.read(Nodes);
	}
};

ISO_DEFUSERCOMPV(FClusterNode, BoundMin, FirstChild, BoundMax, LastChild, FirstInstance, LastInstance, MinInstanceScale, MaxInstanceScale);
ISO_DEFUSERCOMPBV(UHierarchicalInstancedStaticMeshComponent, UInstancedStaticMeshComponent, Nodes);
ObjectLoaderRaw<UHierarchicalInstancedStaticMeshComponent>	load_HierarchicalInstancedStaticMeshComponent("HierarchicalInstancedStaticMeshComponent");
ObjectLoaderRaw<UHierarchicalInstancedStaticMeshComponent>	load_FoliageInstancedStaticMeshComponent("FoliageInstancedStaticMeshComponent");

typedef UObject UFoliageType;

struct FFoliageInstanceBaseInfo {
	TSoftPtr<UActorComponent> BasePtr;
	FVector		CachedLocation;
	FRotator	CachedRotation;
	FVector		CachedDrawScale;

	bool	read(istream_linker& file) {
		return file.read(BasePtr, CachedLocation, CachedRotation, CachedDrawScale);
	}
};
struct FFoliageInstanceBaseCache {
	typedef int32 FFoliageInstanceBaseId;
	FFoliageInstanceBaseId		NextBaseId;
	TMap<FFoliageInstanceBaseId, FFoliageInstanceBaseInfo>	InstanceBaseMap;
	TMap<TPtr<UWorld>, TArray<TPtr<UActorComponent>>>		InstanceBaseLevelMap;
	bool	read(istream_linker& file) {
		return file.read(NextBaseId, InstanceBaseMap, InstanceBaseLevelMap);
	}
};

struct FFoliageInfo {
	enum EFoliageImplType : uint8 {
		Unknown		= 0,
		StaticMesh	= 1,
		Actor		= 2
	};
	uint8				Type;
	TArray<TPtr<AActor>>		ActorInstances;
	FGuid				FoliageTypeUpdateGuid;

	bool	read(istream_linker& file) {
		file.read(Type);
		file.read(ActorInstances);
		return file.read(FoliageTypeUpdateGuid);
	}
};

struct AInstancedFoliageActor : AActor {
	FFoliageInstanceBaseCache InstanceBaseCache;
	TMap<TPtr<UFoliageType>, TKnownPtr<FFoliageInfo>> FoliageInfos;
	bool	read(istream_linker& file) {
		return AActor::read(file) && file.read(InstanceBaseCache) && file.read(FoliageInfos);
	}
};

ISO_DEFUSERCOMPV(FFoliageInstanceBaseInfo, BasePtr, CachedLocation, CachedRotation, CachedDrawScale);
ISO_DEFUSERCOMPV(FFoliageInstanceBaseCache, NextBaseId, InstanceBaseMap, InstanceBaseLevelMap);
ISO_DEFUSERCOMPV(FFoliageInfo, Type, ActorInstances, FoliageTypeUpdateGuid);
ISO_DEFUSERCOMPBV(AInstancedFoliageActor, AActor, InstanceBaseCache, FoliageInfos);
ObjectLoaderRaw<AInstancedFoliageActor>	load_InstancedFoliageActor("InstancedFoliageActor");

//-----------------------------------------------------------------------------
//	Skeletal Mesh
//-----------------------------------------------------------------------------
typedef uint16	FBoneIndexType;

struct FSkeletalMeshImportData {
	struct FMeshWedge {
		uint32			iVertex;
		FVector2D		UVs[4];
		FColor			Color;
	};
	struct FMeshFace {
		uint32			iWedge[3];
		uint16			MeshMaterialIndex;
		FVector			TangentX[3], TangentY[3], TangentZ[3];
		uint32			SmoothingGroups;
	};
	struct FJointPos {
		FTransform	Transform;
	};
	struct FTriangle {
		uint8			MatIndex;
		uint8			AuxMatIndex;
		uint32			SmoothingGroups;
		uint32			WedgeIndex[3];
		FVector			TangentX[3], TangentY[3], TangentZ[3];
	};
	struct FVertInfluence {
		float			Weight;
		uint32			VertIndex;
		FBoneIndexType	BoneIndex;
	};
	struct FMaterial {
		FString MaterialImportName;
		bool read(istream_ref &file) {
			return file.read(MaterialImportName);
		}
	};
	struct FBone {
		FString		Name;
		uint32		Flags;        // reserved / 0x02 = bone where skin is to be attached...	
		int32 		NumChildren;  // children  // only needed in animation ?
		int32       ParentIndex;  // 0/NULL if this is the root bone.  
		FJointPos	BonePos;      // reference position
		bool read(istream_ref &file) {
			return file.read(Name, Flags, NumChildren, ParentIndex, BonePos);
		}
	};
	struct FRawBoneInfluence {
		float		Weight;
		int32		VertexIndex;
		int32		BoneIndex;
	};
	struct FVertex {
		uint32		VertexIndex;	// Index to a vertex
		FVector2D	UVs[4];		// Scaled to BYTES, rather...-> Done in digestion phase, on-disk size doesn't matter here
		FColor		Color;			// Vertex colors
		uint8		MatIndex;		// At runtime, this one will be implied by the face that's pointing to us
		uint8		Reserved;		// Top secret
		FVertex() { clear(*this); }
		bool read(istream_ref &file) {
			return file.read(VertexIndex, Color, MatIndex, Reserved, UVs);
		}
	};
	TArray<FMaterial>	Materials;
	TArray<FVector>		Points;
	TArray<FVertex>		Wedges;
	TArray<FTriangle>	Faces;
	TArray<FBone>		RefBonesBinary;
	TArray<FRawBoneInfluence> Influences;
	TArray<int32>		PointToRawMap;	// Mapping from current point index to the original import point index
	uint32				NumTexCoords; // The number of texture coordinate sets
	uint32				MaxMaterialIndex; // The max material index found on a triangle
	bool32				bHasVertexColors; // If true there are vertex colors in the imported file
	bool32				bHasNormals; // If true there are normals in the imported file
	bool32				bHasTangents; // If true there are tangents in the imported file
	bool32				bUseT0AsRefPose; // If true, then the pose at time=0 will be used instead of the ref pose
	bool32				bDiffPose; // If true, one of the bones has a different pose at time=0 vs the ref pose

	TArray<FSkeletalMeshImportData> MorphTargets;
	TArray<TSet<uint32>>			MorphTargetModifiedPoints;
	TArray<FString>					MorphTargetNames;

	TArray<FSkeletalMeshImportData> AlternateInfluences;
	TArray<FString>					AlternateInfluenceProfileNames;

	bool	read(istream_ref file) {
		int32 Version			= file.get();
		int32 LicenseeVersion	= file.get();
		file.read(bDiffPose, bHasNormals, bHasTangents, bHasVertexColors, bUseT0AsRefPose, MaxMaterialIndex, NumTexCoords, Faces, Influences, Materials, Points, PointToRawMap, RefBonesBinary, Wedges);
		return file.read(MorphTargets, MorphTargetModifiedPoints, MorphTargetNames, AlternateInfluences, AlternateInfluenceProfileNames);
	}
};
ISO_DEFUSERCOMPV(FSkeletalMeshImportData::FMeshWedge, iVertex, UVs, Color);
ISO_DEFUSERCOMPV(FSkeletalMeshImportData::FMeshFace, iWedge, MeshMaterialIndex, TangentX, TangentY, TangentZ, SmoothingGroups);
ISO_DEFUSERCOMPV(FSkeletalMeshImportData::FJointPos, Transform);
ISO_DEFUSERCOMPV(FSkeletalMeshImportData::FTriangle, WedgeIndex, MatIndex, AuxMatIndex, SmoothingGroups, TangentX, TangentY, TangentZ);
ISO_DEFUSERCOMPV(FSkeletalMeshImportData::FVertInfluence, Weight, VertIndex, BoneIndex);
ISO_DEFUSERCOMPV(FSkeletalMeshImportData::FMaterial, MaterialImportName);
ISO_DEFUSERCOMPV(FSkeletalMeshImportData::FBone, Name, Flags, NumChildren, ParentIndex, BonePos);
ISO_DEFUSERCOMPV(FSkeletalMeshImportData::FRawBoneInfluence, Weight, VertexIndex, BoneIndex);
ISO_DEFUSERCOMPV(FSkeletalMeshImportData::FVertex, VertexIndex, UVs, Color, MatIndex);
ISO_DEFUSERCOMPV(FSkeletalMeshImportData,
	Materials, Points, Wedges, Faces, RefBonesBinary, Influences, PointToRawMap, NumTexCoords, MaxMaterialIndex, bHasVertexColors, bHasNormals, bHasTangents, bUseT0AsRefPose, bDiffPose,
	MorphTargets, MorphTargetModifiedPoints, MorphTargetNames, AlternateInfluences, AlternateInfluenceProfileNames
);

struct FRawSkeletalMeshBulkData {
	FBulkData		BulkData;
	FGuid			Guid;
	bool32			bGuidIsHash;
	uint8			GeoImportVersion;
	uint8			SkinningImportVersion;
	bool	read(istream_linker& file) {
		return file.read(GeoImportVersion, SkinningImportVersion, BulkData, Guid, bGuidIsHash);
	}
};

struct USkeletalMeshEditorData : UObject {
	dynamic_array<malloc_block> RawSkeletalMeshBulkDatas;
	dynamic_array<FSkeletalMeshImportData>	SkeletalMeshImportData;
	bool	read(istream_linker& file) {
		UObject::read(file);
		uint32	n = file.get<uint32>();
		for (int i = 0; i < n; i++) {
			auto	data = file.get<FRawSkeletalMeshBulkData>();
			make_save_pos(file), SkeletalMeshImportData.push_back().read(data.BulkData.reader(file, file.bulk_data()));
		}
		return true;
	}
};
ISO_DEFUSERCOMPBV(USkeletalMeshEditorData, UObject, SkeletalMeshImportData);
ObjectLoaderRaw<USkeletalMeshEditorData>	load_SkeletalMeshEditorData("SkeletalMeshEditorData");


struct FWeightedRandomSampler {
	TArray<float>	Prob;
	TArray<int32>	Alias;
	float			TotalWeight;
	bool	read(istream_ref file) {
		return file.read(Prob, Alias, TotalWeight);
	}
};

struct FSkeletalMeshAreaWeightedTriangleSampler : FWeightedRandomSampler {
};

struct FSkeletalMeshSamplingLODBuiltData {
	FSkeletalMeshAreaWeightedTriangleSampler AreaWeightedTriangleSampler;
	bool	read(istream_linker& file) {
		return file.read(AreaWeightedTriangleSampler);
	}
};
ObjectLoaderRaw<FSkeletalMeshSamplingLODBuiltData>	load_SkeletalMeshSamplingLODBuiltData("SkeletalMeshSamplingLODBuiltData");

struct FMeshBoneInfo {
	FName2		Name;
	int32		ParentIndex;
	FString		ExportName;
	bool	read(istream_linker& file) {
		return file.read(Name, ParentIndex, ExportName);
	}
};

struct FReferenceSkeleton {
	TArray<FMeshBoneInfo>	RefBoneInfo;
	TArray<FTransform>		RefBonePose;
	TMap<FName2, int32>		NameToIndexMap;
	bool	read(istream_linker& file) {
		return file.read(RefBoneInfo, RefBonePose, NameToIndexMap);
	}
};

struct FClothingSectionData {
	FGuid		AssetGuid;
	int32		AssetLodIndex;
	bool	read(istream_linker& file) {
		return file.read(AssetGuid) && file.read(AssetLodIndex);
	}
};
struct FMeshToMeshVertData {
	FVector4	PositionBaryCoordsAndDist;
	FVector4	NormalBaryCoordsAndDist;
	FVector4	TangentBaryCoordsAndDist;
	uint16		SourceMeshVertIndices[4];
	uint32		Padding[2];
};

struct FSoftSkinVertex {
	FVector			Position;
	FVector			TangentX;
	FVector			TangentY;
	FVector4		TangentZ;
	FVector2D		UVs[4];
	FColor			Color;
	FBoneIndexType	InfluenceBones[12];
	uint8			InfluenceWeights[12];
	bool	read(istream_linker& file) {
		return file.read(Position, TangentX, TangentY, TangentZ, UVs, Color, InfluenceBones, InfluenceWeights);
	}
};
struct FSkelMeshSection {
	uint16					MaterialIndex;
	uint32					BaseIndex;
	uint32					NumTriangles;
	bool32					bRecomputeTangent;
	bool32					bCastShadow;
	uint32					BaseVertexIndex;
	TArray<FSoftSkinVertex> SoftVertices;
	TArray<FMeshToMeshVertData> ClothMappingData;
	TArray<FBoneIndexType>	BoneMap;
	int32					NumVertices;
	int32					MaxBoneInfluences;
	bool32					bUse16BitBoneIndex;
	int16					CorrespondClothAssetIndex;
	FClothingSectionData	ClothingData;
	TMap<int32, TArray<int32>> OverlappingVertices;
	bool32					bDisabled;
	int32					GenerateUpToLodIndex;
	int32					OriginalDataSectionIndex;
	int32					ChunkedParentSectionIndex;

	bool	read(istream_linker& file) {
		auto	StripFlags	= file.get<FStripDataFlags>();
		file.read(MaterialIndex);

		if (!StripFlags.IsDataStrippedForServer()) {
			file.read(BaseIndex);
			file.read(NumTriangles);
		}

		file.get<uint8>();	//	Ar << S.bEnableClothLOD_DEPRECATED;
		file.read(bRecomputeTangent);
		file.read(bCastShadow);
		if (!StripFlags.IsDataStrippedForServer())
			file.read(BaseVertexIndex);

		if (!StripFlags.IsEditorDataStripped())
			file.read(SoftVertices);

		return file.read(bUse16BitBoneIndex, BoneMap, NumVertices, MaxBoneInfluences, ClothMappingData, CorrespondClothAssetIndex, ClothingData, OverlappingVertices, bDisabled, GenerateUpToLodIndex, OriginalDataSectionIndex, ChunkedParentSectionIndex);
	}

	void	InitSubmesh(SubMesh* sm, int32 *indices) {
		sm->technique	= ISO::root("data")["default"]["specular"];

		sm->NumFaces(NumTriangles);
		auto	*di	= sm->indices.begin();
		auto	*pi	= indices + BaseIndex;
		for (int i = 0; i < NumTriangles; i++) {
			(*di)[2]	= *pi++;
			(*di)[1]	= *pi++;
			(*di)[0]	= *pi++;
			++di;
		}

		auto	dv		= sm->CreateVerts<UnrealVertex>(NumVertices);

		for (auto &i : SoftVertices) {
			dv->pos.x = i.Position.X;
			dv->pos.y = i.Position.Y;
			dv->pos.z = i.Position.Z;

			dv->norm.x = i.TangentZ.X;
			dv->norm.y = i.TangentZ.Y;
			dv->norm.z = i.TangentZ.Z;
			++dv;
		}
		sm->UpdateExtent();
	}

};

struct FSkelMeshSourceSectionUserData {
	bool32					bRecomputeTangent;
	bool32					bCastShadow;
	int16					CorrespondClothAssetIndex;
	FClothingSectionData	ClothingData;
	bool32					bDisabled;
	int32					GenerateUpToLodIndex;

	bool	read(istream_linker& file) {
		auto	StripFlags	= file.get<FStripDataFlags>();
		return StripFlags.IsEditorDataStripped() || file.read(bRecomputeTangent, bCastShadow, bDisabled, GenerateUpToLodIndex, CorrespondClothAssetIndex, ClothingData);
	}
};

struct FSkeletalMaterial {
	TPtr<UMaterialInterface>	MaterialInterface;
	FName2						MaterialSlotName;
	FName2						ImportedMaterialSlotName;
	FMeshUVChannelInfo			UVChannelData;

	bool	read(istream_linker& file) {
		file.read(MaterialInterface);
		file.read(MaterialSlotName);
		if (file.get<bool32>())
			file.read(ImportedMaterialSlotName);
		return file.read(UVChannelData);
	}
};

struct FVertInfluence {
	float			Weight;
	uint32			VertIndex;
	FBoneIndexType	BoneIndex;
	bool	read(istream_linker& file) { return file.read(Weight, VertIndex, BoneIndex); }
};

struct FRawSkinWeight {
	FBoneIndexType	InfluenceBones[12];
	uint8			InfluenceWeights[12];
	bool	read(istream_linker& file) { return file.read(InfluenceBones) && file.read(InfluenceWeights); }
};

struct FImportedSkinWeightProfileData {
	TArray<FRawSkinWeight> SkinWeights;
	TArray<FVertInfluence> SourceModelInfluences;
	bool	read(istream_linker& file) { return file.read(SkinWeights) && file.read(SourceModelInfluences); }
};

struct FSkeletalMeshLODModel {
	TArray<FSkelMeshSection>	Sections;
	TMap<int32, FSkelMeshSourceSectionUserData>	UserSectionsData;
	TArray<int32>				IndexBuffer;
	TArray<FBoneIndexType>		ActiveBoneIndices;
	TArray<FBoneIndexType>		RequiredBones;
	TArray<int32>				MeshToImportVertexMap;
	int32						MaxImportVertex;
	uint32						NumVertices;
	uint32						NumTexCoords;
	TBulkArrayData<int32>		RawPointIndices;
	FString						RawSkeletalMeshBulkDataID;
	bool32						bIsBuildDataAvailable;
	bool32						bIsRawSkeletalMeshBulkDataEmpty;
	TMap<FName2, FImportedSkinWeightProfileData> SkinWeightProfiles;

	bool	read(istream_linker& file) {
		auto	StripFlags	= file.get<FStripDataFlags>();

		if (StripFlags.IsDataStrippedForServer()) {
			file.get<TArray<FSkelMeshSection>>();
			file.get<TMap<int32, FSkelMeshSourceSectionUserData>>();
			file.get<TArray<int32>>();
			file.get<TArray<FBoneIndexType>>();
		} else {
			file.read(Sections);

			if (!StripFlags.IsEditorDataStripped()) {
				file.read(UserSectionsData);
				file.read(IndexBuffer);
			}
			file.read(ActiveBoneIndices);
		}

		file.get<uint32>();

		if (!StripFlags.IsDataStrippedForServer())
			file.read(NumVertices);

		file.read(RequiredBones);

		if (!StripFlags.IsEditorDataStripped()) {
			file.read(RawPointIndices);
			file.read(RawSkeletalMeshBulkDataID);
			file.read(bIsBuildDataAvailable);
			file.read(bIsRawSkeletalMeshBulkDataEmpty);
		}

		file.read(MeshToImportVertexMap);
		file.read(MaxImportVertex);

		if (!StripFlags.IsDataStrippedForServer())
			file.read(NumTexCoords);

		return file.read(SkinWeightProfiles);
	}
};

struct FSkeletalMeshModel {
	TArray<TKnownPtr<FSkeletalMeshLODModel>> LODModels;
	FGuid	SkeletalMeshModelGUID;
	bool32	bGuidIsHash;
	TArray<TBulkArrayData<uint8>> OriginalReductionSourceMeshData;

	bool	read(istream_linker& file) {
		auto	StripFlags	= file.get<FStripDataFlags>();
		file.read(LODModels);
		file.read(SkeletalMeshModelGUID);
		file.read(bGuidIsHash);
		if (!StripFlags.IsEditorDataStripped())
			file.read(OriginalReductionSourceMeshData);
		return true;
	}
};

struct USkeletalMesh : UObject {
	FBoxSphereBounds				ImportedBounds;
	TArray<FSkeletalMaterial>		Materials;
	FReferenceSkeleton				RefSkeleton;
	TKnownPtr<FSkeletalMeshModel>	ImportedModel;
	ISO_ptr<Model3>					Model;

	bool	read(istream_linker& file) {
		UObject::read(file);
		auto	StripFlags	= file.get<FStripDataFlags>();
		file.read(ImportedBounds);
		file.read(Materials);
		file.read(RefSkeleton);

		if (!StripFlags.IsEditorDataStripped())
			file.read(ImportedModel);

		auto	bCooked		= file.get<bool32>();

		file.get<TArray<UObject*>>();

		Model.Create(ISO::Browser((*this)["Name"]).GetString());
		auto	lod = ImportedModel->LODModels[0];
		for (auto &i : lod->Sections) {
			ISO_ptr<SubMesh>	mesh(0);
			Model->submeshes.Append(mesh);
			i.InitSubmesh( mesh, lod->IndexBuffer);
		}
		Model->UpdateExtents();

		return true;
	}
};

ISO_DEFUSERCOMPV(FMeshBoneInfo, Name, ParentIndex, ExportName);
ISO_DEFUSERCOMPV(FReferenceSkeleton, RefBoneInfo, RefBonePose, NameToIndexMap);

ISO_DEFUSERCOMPV(FClothingSectionData, AssetGuid, AssetLodIndex);
ISO_DEFUSERCOMPV(FMeshToMeshVertData, PositionBaryCoordsAndDist, NormalBaryCoordsAndDist, TangentBaryCoordsAndDist, SourceMeshVertIndices);
ISO_DEFUSERCOMPV(FSoftSkinVertex, Position, TangentX, TangentY, TangentZ, UVs, Color, InfluenceBones,InfluenceWeights);
ISO_DEFUSERCOMPV(FSkelMeshSection, MaterialIndex, BaseIndex, NumTriangles, bRecomputeTangent, bCastShadow, BaseVertexIndex, SoftVertices, ClothMappingData, BoneMap, NumVertices, MaxBoneInfluences, bUse16BitBoneIndex, CorrespondClothAssetIndex, ClothingData, OverlappingVertices, bDisabled, GenerateUpToLodIndex, OriginalDataSectionIndex, ChunkedParentSectionIndex);

ISO_DEFUSERCOMPV(FSkelMeshSourceSectionUserData, bRecomputeTangent, bCastShadow, CorrespondClothAssetIndex, ClothingData, bDisabled, GenerateUpToLodIndex);
ISO_DEFUSERCOMPV(FVertInfluence, Weight, VertIndex, BoneIndex);
ISO_DEFUSERCOMPV(FRawSkinWeight, InfluenceBones, InfluenceWeights);
ISO_DEFUSERCOMPV(FImportedSkinWeightProfileData, SkinWeights, SourceModelInfluences);
ISO_DEFUSERCOMPV(FSkeletalMeshLODModel, Sections, UserSectionsData, IndexBuffer, ActiveBoneIndices, RequiredBones, MeshToImportVertexMap, MaxImportVertex, NumVertices, NumTexCoords, RawPointIndices, RawSkeletalMeshBulkDataID, bIsBuildDataAvailable, bIsRawSkeletalMeshBulkDataEmpty, SkinWeightProfiles);

ISO_DEFUSERCOMPV(FSkeletalMaterial, MaterialInterface, MaterialSlotName, ImportedMaterialSlotName, UVChannelData);
ISO_DEFUSERCOMPV(FSkeletalMeshModel, LODModels, SkeletalMeshModelGUID, bGuidIsHash, OriginalReductionSourceMeshData);

ISO_DEFUSERCOMPBV(USkeletalMesh, UObject, ImportedBounds, Materials, RefSkeleton, ImportedModel, Model);
ISO_DEFUSERCOMPV(FWeightedRandomSampler, Prob, Alias, TotalWeight);
ISO_DEFUSER(FSkeletalMeshAreaWeightedTriangleSampler, FWeightedRandomSampler);
ISO_DEFUSERCOMPV(FSkeletalMeshSamplingLODBuiltData, AreaWeightedTriangleSampler);

ObjectLoaderRaw<USkeletalMesh>	load_SkeletalMesh("SkeletalMesh");

struct FAnimCurveType {
	bool32 bMaterial;
	bool32 bMorphtarget;
	bool	read(istream_linker& file) {
		return file.read(bMaterial) && file.read(bMorphtarget);
	}
};

struct FBoneReference {
	FName2 BoneName;
	bool	read(istream_linker& file) {
		return file.read(BoneName);
	}
};

struct FCurveMetaData {
	FAnimCurveType			Type;
	TArray<FBoneReference>	LinkedBones;
	uint8					MaxLOD;
	bool	read(istream_linker& file) {
		return file.read(Type, LinkedBones, MaxLOD);
	}
};

struct FSmartNameMapping {
	TMap<FName, FCurveMetaData> CurveMetaDataMap;
	bool	read(istream_linker& file) {
		return file.read(CurveMetaDataMap);
	}
};

struct FSmartNameContainer {
	TMap<FName, FSmartNameMapping> NameMappings;	// List of smartname mappings
	bool	read(istream_linker& file) {
		return file.read(NameMappings);
	}
};

struct FReferencePose {
	FName	PoseName;
	TArray<FTransform>	ReferencePose;
	TPtr<USkeletalMesh> SourceReferenceMesh;

	bool	read(istream_linker& file) {
		file.read(PoseName);
		file.read(ReferencePose);
		file.read(SourceReferenceMesh);
		return true;
	}
};

struct USkeleton : UObject {
	FReferenceSkeleton			ReferenceSkeleton;
	TMap<FName, FReferencePose> AnimRetargetSources;
	FGuid						Guid;
	FSmartNameContainer			SmartNames;
	TArray<FName>				ExistingMarkerNames;

	bool	read(istream_linker& file) {
		UObject::read(file);
		file.read(ReferenceSkeleton);
		file.read(AnimRetargetSources);
		file.read(Guid);

		file.read(SmartNames);
		auto	strip = file.get<FStripDataFlags>();
		if (!strip.IsEditorDataStripped())
			file.read(ExistingMarkerNames);
		return true;
	}
};

ISO_DEFUSERCOMPV(FAnimCurveType, bMaterial, bMorphtarget);
ISO_DEFUSERCOMPV(FBoneReference, BoneName);
ISO_DEFUSERCOMPV(FCurveMetaData, Type, LinkedBones, MaxLOD);
ISO_DEFUSERCOMPV(FSmartNameMapping, CurveMetaDataMap);
ISO_DEFUSERCOMPV(FSmartNameContainer, NameMappings);
ISO_DEFUSERCOMPV(FReferencePose, PoseName, ReferencePose, SourceReferenceMesh);
ISO_DEFUSERCOMPBV(USkeleton, UObject, ReferenceSkeleton, AnimRetargetSources, Guid, SmartNames, ExistingMarkerNames);
ObjectLoaderRaw<USkeleton>	load_Skeleton("Skeleton");

struct FNodeItem {
	FName		ParentName;
	FTransform	Transform;
};
struct UNodeMappingContainer : public UObject {
	TMap<FName, FNodeItem>	SourceItems;
	TMap<FName, FNodeItem>	TargetItems;
	TMap<FName, FName>		SourceToTarget;
	TPtr<UObject>			SourceAsset; 
	TPtr<UObject>			TargetAsset;
};

ISO_DEFUSERCOMPV(FNodeItem, ParentName, Transform);
ISO_DEFUSERCOMPBV(UNodeMappingContainer, UObject, SourceItems, TargetItems, SourceToTarget, SourceAsset, TargetAsset);

//-----------------------------------------------------------------------------
//	model
//-----------------------------------------------------------------------------

typedef AActor	ABrush;

struct FLightmassPrimitiveSettings {
	bool32	bUseTwoSidedLighting;
	bool32	bShadowIndirectOnly;
	bool32	bUseEmissiveForStaticLighting;
	bool32	bUseVertexNormalForHemisphereGather;
	float	EmissiveLightFalloffExponent;
	float	EmissiveLightExplicitInfluenceRadius;
	float	EmissiveBoost;
	float	DiffuseBoost;
	float	FullyOccludedSamplesFraction;
	bool	read(istream_linker& file) {
		return iso::read(file,
			bUseTwoSidedLighting, bShadowIndirectOnly, FullyOccludedSamplesFraction, bUseEmissiveForStaticLighting, bUseVertexNormalForHemisphereGather,
			EmissiveLightFalloffExponent, EmissiveLightExplicitInfluenceRadius,
			EmissiveBoost, DiffuseBoost
		);
	}
};

struct FBspNode {
	FPlane		Plane;
	int32		iVertPool;
	int32		iSurf;
	int32		iVertexIndex;
	uint16		ComponentIndex;
	uint16		ComponentNodeIndex;
	int32		ComponentElementIndex;
	union { int32 iBack; int32 iChild[1]; };
	int32		iFront;
	int32		iPlane;
	int32		iCollisionBound;// 4  Collision bound
	uint8		iZone[2];		// 2  Visibility zone in 1=front, 0=back
	uint8		NumVertices;	// 1  Number of vertices in node
	uint8		NodeFlags;		// 1  Node flags
	int32		iLeaf[2];		// 8  Leaf in back and front, INDEX_NONE=not a leaf
};

struct FBspSurf {
	TPtr<UMaterialInterface>	Material;		// 4 Material
	uint32				PolyFlags;		// 4 Polygon flags
	int32				pBase;			// 4 Polygon & texture base point index (where U,V==0,0)
	int32				vNormal;		// 4 Index to polygon normal
	int32				vTextureU;		// 4 Texture U-vector index
	int32				vTextureV;		// 4 Texture V-vector index
	int32				iBrushPoly;		// 4 Editor brush polygon index
	TPtr<ABrush>		Actor;			// 4 Brush actor owning this Bsp surface
	FPlane				Plane;			// 16 The plane this surface lies on
	float				LightMapScale;	// 4 The number of units/lightmap texel on this surface
	int32				iLightmassIndex;// 4 Index to the lightmass settings
	
	bool	read(istream_linker& file) {
		return iso::read(file,
			Material, PolyFlags, pBase, vNormal,
			vTextureU, vTextureV,
			iBrushPoly,
			Actor,
			Plane,
			LightMapScale, iLightmassIndex
		);
	}
};

struct FLeaf {
	int32		iZone;
};

struct FVert {
	int32 		pVertex;	// Index of vertex
	int32		iSide;		// If shared, index of unique side. Otherwise INDEX_NONE
	FVector2D	ShadowTexCoord;
	FVector2D	BackfaceShadowTexCoord;
};

struct FPoly {
	FVector				Base;					// Base point of polygon
	FVector				Normal;					// Normal of polygon
	FVector				TextureU;				// Texture U vector
	FVector				TextureV;				// Texture V vector
	TArray<FVector>		Vertices;
	uint32				PolyFlags;				// FPoly & Bsp poly bit flags (PF_)
	TPtr<ABrush>				Actor;			// Brush where this originated, or NULL
	TPtr<UMaterialInterface>	Material;
	FName				RulesetVariation;		// Name of variation within a ProcBuilding Ruleset for this face
	FName				ItemName;				// Item name
	int32				iLink;					// iBspSurf, or brush fpoly index of first identical polygon, or MAX_uint16
	int32				iLinkSurf;
	int32				iBrushPoly;				// Index of editor solid's polygon this originated from
	uint32				SmoothingMask;			// A mask used to determine which smoothing groups this polygon is in.  SmoothingMask & (1 << GroupNumber)
	float				LightMapScale;			// The number of units/shadowmap texel on this surface
	FLightmassPrimitiveSettings		LightmassSettings;

	bool	read(istream_linker& file) {
		return iso::read(file,
			Base, Normal,
			TextureU, TextureV,
			Vertices, PolyFlags, Actor,
			ItemName, Material, iLink, iBrushPoly,
			LightMapScale, LightmassSettings, RulesetVariation
		);
	}
};

struct UPolys : public UObject {
	TArray<FPoly> Element;
	bool	read(istream_linker& file) {
		return UObject::read(file) && file.read(Element);
	}
};

ISO_DEFUSERCOMPV(FLightmassPrimitiveSettings, bUseTwoSidedLighting, bShadowIndirectOnly, bUseEmissiveForStaticLighting, bUseVertexNormalForHemisphereGather, EmissiveLightFalloffExponent, EmissiveLightExplicitInfluenceRadius, EmissiveBoost, DiffuseBoost, FullyOccludedSamplesFraction);
ISO_DEFUSERCOMPV(FBspNode, Plane, iVertPool, iSurf, iVertexIndex, ComponentIndex, ComponentNodeIndex, ComponentElementIndex, iChild, iFront, iPlane, iCollisionBound, iZone, NumVertices, NodeFlags,iLeaf);
ISO_DEFUSERCOMPV(FBspSurf, Material, PolyFlags, pBase, vNormal, vTextureU, vTextureV, iBrushPoly, Actor, Plane, LightMapScale, iLightmassIndex);
ISO_DEFUSER(FLeaf, int32);
ISO_DEFUSERCOMPV(FVert, pVertex, iSide, ShadowTexCoord, BackfaceShadowTexCoord);
ISO_DEFUSERCOMPV(FPoly, Base, Normal, TextureU, TextureV, Vertices, PolyFlags, Actor, Material, RulesetVariation, ItemName, iLink, iLinkSurf, iBrushPoly, SmoothingMask, LightMapScale, LightmassSettings);
ISO_DEFUSERCOMPBV(UPolys, UObject, Element);
ObjectLoaderRaw<UPolys>	load_Polys("Polys");


struct FModelVertex {
	FVector		Position;
	FVector		TangentX;
	FVector4	TangentZ;
	FVector2D	TexCoord;
	FVector2D	ShadowTexCoord;
};

struct UModel : UObject {
	FBoxSphereBounds			Bounds;
	int32						NumSharedSides;
	bool32						RootOutside;
	bool32						Linked;
	uint32						NumUniqueVertices;

	TBulkArray<FBspNode>		Nodes;
	TBulkArray<FVert>			Verts;
	TBulkArray<FVector>			Vectors;
	TBulkArray<FVector>			Points;
	TArray<FBspSurf>			Surfs;
	TPtr<UPolys>				Polys;
	TBulkArray<int32>			LeafHulls;
	TBulkArray<FLeaf>			Leaves;
	
	TArray<FModelVertex>		VertexBuffer;
	FGuid						LightingGuid;
	TArray<FLightmassPrimitiveSettings>	LightmassSettings;

	bool	read(istream_linker& file) {
		UObject::read(file);
		auto	strip	= file.get<FStripDataFlags>();
		return iso::read(file,
			Bounds, Vectors, Points, Nodes, Surfs, Verts,
			NumSharedSides, Polys, LeafHulls, Leaves,
			RootOutside, Linked,
			NumUniqueVertices,
			VertexBuffer,
			LightingGuid, LightmassSettings
		);
	}

};

ISO_DEFUSERCOMPV(FModelVertex,	Position, TangentX, TangentZ, TexCoord, ShadowTexCoord);
ISO_DEFUSERCOMPBV(UModel,		UObject, Bounds, NumSharedSides, RootOutside, Linked, NumUniqueVertices, Nodes, Verts, Vectors, Points, Surfs, Polys, LeafHulls, Leaves, VertexBuffer, LightingGuid, LightmassSettings);
ObjectLoaderRaw<UModel>			load_Model("Model");

struct FModelElement {
	TPtr<UObject>	Component;
	TPtr<UObject>	Material;
	TArray<uint16>	Nodes;
	FGuid			MapBuildDataId;
	bool	read(istream_linker& file) {
		return file.read( MapBuildDataId, Component, Material, Nodes);
	}
};

struct UModelComponent : UObject {
	TPtr<UModel>	Model;
	int32			ComponentIndex;
	TArray<uint16>	Nodes;
	TArray<FModelElement> Elements;
	bool	read(istream_linker& file) {
		return UObject::read(file) && file.read(Model, Elements, ComponentIndex, Nodes);
	}
};

ISO_DEFUSERCOMPV(FModelElement,		Component, Material, Nodes, MapBuildDataId);
ISO_DEFUSERCOMPBV(UModelComponent,	UObject, Model, ComponentIndex, Nodes, Elements);
ObjectLoaderRaw<UModelComponent>	load_ModelComponent("ModelComponent");


struct UHoudiniStaticMesh : UObject {
	TBulkArray<FVector>		VertexPositions;
	TBulkArray<FIntVector>	TriangleIndices;
	TBulkArray<FColor>		VertexInstanceColors;
	TBulkArray<FVector>		VertexInstanceNormals;
	TBulkArray<FVector>		VertexInstanceUTangents;
	TBulkArray<FVector>		VertexInstanceVTangents;
	TBulkArray<FVector2D>	VertexInstanceUVs;
	TBulkArray<int32>		MaterialIDsPerTriangle;

	bool read(istream_linker &file) {
		return UObject::read(file)
		&& iso::read(file,
			VertexPositions,
			TriangleIndices,
			VertexInstanceColors,
			VertexInstanceNormals,
			VertexInstanceUTangents,
			VertexInstanceVTangents,
			VertexInstanceUVs,
			MaterialIDsPerTriangle
		);
	}

};
ISO_DEFUSERCOMPBV(UHoudiniStaticMesh,	UObject, VertexPositions, TriangleIndices, VertexInstanceColors, VertexInstanceNormals, VertexInstanceUTangents, VertexInstanceVTangents, VertexInstanceUVs, MaterialIDsPerTriangle);
ObjectLoaderRaw<UHoudiniStaticMesh>	load_HoudiniStaticMesh("HoudiniStaticMesh");

//-----------------------------------------------------------------------------
//	sound
//-----------------------------------------------------------------------------

struct USoundWave : UObject {
	ISO_ptr<void>	Sample;
	FGuid			CompressedDataGuid;

	bool read(istream_linker &file) {
		UObject::read(file);
		auto	cooked	= file.get<bool32>();
		auto	bulk	= file.get<FBulkData>();
		Sample = FileHandler::Get("wav")->Read(none, bulk.reader(file, file.bulk_data()));
		return file.read(CompressedDataGuid);
	}
};
ISO_DEFUSERCOMPBV(USoundWave,	UObject, Sample, CompressedDataGuid);
ObjectLoaderRaw<USoundWave>		load_SoundWave("SoundWave");

//-----------------------------------------------------------------------------
//	graph
//-----------------------------------------------------------------------------

enum class EPinContainerType : uint8 {
	None,
	Array,
	Set,
	Map
};
struct FEdGraphTerminalType {
	FName2			TerminalCategory;
	FName2			TerminalSubCategory;
	TPtr<UObject>	TerminalSubCategoryObject;
	bool			bTerminalIsConst;
	bool			bTerminalIsWeakPointer;
	bool read(istream_linker& file) {
		return file.read(
			TerminalCategory,
			TerminalSubCategory,
			TerminalSubCategoryObject,
			bTerminalIsConst,
			bTerminalIsWeakPointer
		);
	}
};

struct FSimpleMemberReference {
	TPtr<UObject>	MemberParent;
	FName2			MemberName;
	FGuid			MemberGuid;
	bool read(istream_linker& file) {
		return file.read(MemberParent, MemberName, MemberGuid);
	}
};

struct FEdGraphPinType {
	FName2					PinCategory;
	FName2					PinSubCategory;
	TPtr<UObject>			PinSubCategoryObject;
	FSimpleMemberReference	PinSubCategoryMemberReference;
	FEdGraphTerminalType	PinValueType;
	EPinContainerType		ContainerType;
	bool32					bIsReference;
	bool32					bIsConst;
	bool32					bIsWeakPointer;

	bool read(istream_linker& file) {
		file.read(PinCategory, PinSubCategory, PinSubCategoryObject, ContainerType);
		if (ContainerType == EPinContainerType::Map)
			file.read(PinValueType);

		return file.read(bIsReference, bIsWeakPointer, PinSubCategoryMemberReference, bIsConst);
	}
};

ISO_DEFUSER(EPinContainerType, uint8);
ISO_DEFUSERCOMPV(FEdGraphTerminalType,	TerminalCategory, TerminalSubCategory, TerminalSubCategoryObject, bTerminalIsConst, bTerminalIsWeakPointer);
ISO_DEFUSERCOMPV(FSimpleMemberReference, MemberParent, MemberName, MemberGuid);
ISO_DEFUSERCOMPV(FEdGraphPinType,		 PinCategory, PinSubCategory, PinSubCategoryObject, PinSubCategoryMemberReference, PinValueType, ContainerType, bIsReference, bIsConst, bIsWeakPointer);
ObjectLoaderRaw<FEdGraphPinType>		load_EdGraphPinType("EdGraphPinType");

//-----------------------------------------------------------------------------
//	ULevel
//-----------------------------------------------------------------------------

struct FPrecomputedVisibilityCell {
	FVector	Min;
	uint16	ChunkIndex;
	uint16	DataOffset;
};

struct FCompressedVisibilityChunk {
	bool			bCompressed;
	int32			UncompressedSize;
	TArray<uint8>	Data;
	bool read(istream_ref file) {
		return file.read(bCompressed, UncompressedSize, Data);
	}
};

struct FPrecomputedVisibilityBucket {
	int32								CellDataSize;
	TArray<FPrecomputedVisibilityCell>	Cells;
	TArray<FCompressedVisibilityChunk>	CellDataChunks;
	bool read(istream_ref file) {
		return file.read(CellDataSize, Cells, CellDataChunks);
	}
};
struct FPrecomputedVisibilityHandler {
	FVector2D	CellBucketOriginXY;
	float		CellSizeXY;
	float		CellSizeZ;
	int32		CellBucketSizeXY;
	int32		NumCellBuckets;
	int32		Id;
	TArray<FPrecomputedVisibilityBucket> CellBuckets;
	bool read(istream_ref file) {
		return iso::read(file,
			CellBucketOriginXY,
			CellSizeXY,
			CellSizeZ,
			CellBucketSizeXY,
			NumCellBuckets,
			CellBuckets
		);
	}
};

struct FPrecomputedVolumeDistanceField {
	float	VolumeMaxDistance;
	FBox	VolumeBox;
	int32	VolumeSizeX;
	int32	VolumeSizeY;
	int32	VolumeSizeZ;
	TArray<FColor> Data;
	bool read(istream_ref file) {
		return file.read(VolumeMaxDistance, VolumeBox, VolumeSizeX, VolumeSizeY, VolumeSizeZ, Data);
	}
};

struct FVolumetricLightmapDataLayer {
	TArray<uint8> Data;
	FString		  PixelFormatString;
	bool		  read(istream_ref file) {
		 return file.read(Data) && file.read(PixelFormatString);
	}
};

class FVolumetricLightmapBrickData {
public:
	FVolumetricLightmapDataLayer AmbientVector;
	FVolumetricLightmapDataLayer SHCoefficients[6];
	FVolumetricLightmapDataLayer SkyBentNormal;
	FVolumetricLightmapDataLayer DirectionalLightShadowing;
	FVolumetricLightmapDataLayer LQLightColor;
	FVolumetricLightmapDataLayer LQLightDirection;

	bool read(istream_ref file){
		return file.read(AmbientVector, SHCoefficients, SkyBentNormal, DirectionalLightShadowing, LQLightColor, LQLightDirection);
	}
};

struct FPrecomputedVolumetricLightmapData {
	FBox Bounds;

	FIntVector IndirectionTextureDimensions;
	FVolumetricLightmapDataLayer IndirectionTexture;

	int32							BrickSize;
	FIntVector						BrickDataDimensions;
	FVolumetricLightmapBrickData	BrickData;

	TArray<FIntVector>	SubLevelBrickPositions;
	TArray<FColor>		IndirectionTextureOriginalValues;

	int32 BrickDataBaseOffsetInAtlas;
	TArray<FPrecomputedVolumetricLightmapData*> SceneDataAdded;

	TArray<uint8> CPUSubLevelIndirectionTable;
	TSparseArray<FPrecomputedVolumetricLightmapData*> CPUSubLevelBrickDataList;
	int32 IndexInCPUSubLevelBrickDataList;

	bool read(istream_ref file) {
		return file.read(Bounds, IndirectionTextureDimensions, IndirectionTexture, BrickSize, BrickDataDimensions, BrickData, SubLevelBrickPositions, IndirectionTextureOriginalValues);
	}
};

struct FPerInstanceLightmapData {
	FVector2D LightmapUVBias;
	FVector2D ShadowmapUVBias;
};

class FLightMap {
public:
	enum {
		LMT_None = 0,
		LMT_1D	 = 1,
		LMT_2D	 = 2,
	};

	TArray<FGuid> LightGuids;

	bool read(istream_linker& file) {
		return file.read(LightGuids);
	}
};

struct FLegacyLightMap1D : FLightMap {
	bool read(istream_linker& file){
		return FLightMap::read(file);
		/*
		UObject* Owner;

		FUntypedBulkData2<FQuantizedDirectionalLightSample> DirectionalSamples;
		FUntypedBulkData2<FQuantizedSimpleLightSample>		SimpleSamples;

		Ar << Owner;

		DirectionalSamples.Serialize(Ar, Owner, INDEX_NONE, false);

		for (int32 ElementIndex = 0; ElementIndex < 5; ElementIndex++) {
			FVector Dummy;
			Ar << Dummy;
		}

		SimpleSamples.Serialize(Ar, Owner, INDEX_NONE, false);
		*/
	}
};

struct FLightMap2D : FLightMap {
	TPtr<ULightMapTexture2D>		Textures[2];
	TPtr<ULightMapTexture2D>		SkyOcclusionTexture;
	TPtr<ULightMapTexture2D>		AOMaterialMaskTexture;
	TPtr<UShadowMapTexture2D>		ShadowMapTexture;
	TPtr<ULightMapVirtualTexture2D>	VirtualTexture;
	pair<FVector4,FVector4>			ScaleAddVectors[4];
	FVector2D						CoordinateScale;
	FVector2D						CoordinateBias;
	FVector4						InvUniformPenumbraSize;
	bool32							bShadowChannelValid[4];

	bool read(istream_linker& file){
		return FLightMap::read(file)
			&& file.read(Textures, SkyOcclusionTexture, AOMaterialMaskTexture, ScaleAddVectors, CoordinateScale, CoordinateBias, bShadowChannelValid, InvUniformPenumbraSize, VirtualTexture);
	}
};

struct FShadowMap {
public:
	enum {
		SMT_None = 0,
		SMT_2D	 = 2,
	};

	TArray<FGuid> LightGuids;
	
	bool read(istream_linker& file) {
		return file.read(LightGuids);
	}
};

struct FShadowMap2D : FShadowMap {
	TPtr<UShadowMapTexture2D>	Texture;
	FVector2D					CoordinateScale;
	FVector2D					CoordinateBias;
	bool32						bChannelValid[4];
	FVector4					InvUniformPenumbraSize;

	bool read(istream_linker& file) {
		return FShadowMap::read(file) && file.read(Texture, CoordinateScale, CoordinateBias, bChannelValid, InvUniformPenumbraSize);
	}
};

struct FMeshMapBuildData {
	TKnownPtr<FLightMap>	LightMap;
	TKnownPtr<FShadowMap>	ShadowMap;
	TArray<FGuid>			IrrelevantLights;
	TBulkArray<FPerInstanceLightmapData> PerInstanceLightmapData;

	bool read(istream_linker& file) {
		switch (file.get<uint32>()) {
			default:
			case FLightMap::LMT_None:
				break;
			case FLightMap::LMT_1D:
				LightMap = file.get<TKnownPtr<FLegacyLightMap1D>>();
				break;
			case FLightMap::LMT_2D:
				LightMap = file.get<TKnownPtr<FLightMap2D>>();
				break;
		}
		switch (file.get<uint32>()) {
			case FShadowMap::SMT_None:
				break;
			case FShadowMap::SMT_2D:
				ShadowMap = file.get<TKnownPtr<FShadowMap2D>>();
				break;
		}

		return file.read(IrrelevantLights, PerInstanceLightmapData);
	}
};

struct FStaticShadowDepthMapData {
	FMatrix				WorldToLight;
	int32				ShadowMapSizeX;
	int32				ShadowMapSizeY;
	TArray<float16>		DepthSamples;

	bool read(istream_linker& file) {
		return file.read(WorldToLight, ShadowMapSizeX, ShadowMapSizeY, DepthSamples);
	}
};

struct FLightComponentMapBuildData {
	int32					  ShadowMapChannel;
	FStaticShadowDepthMapData DepthMap;

	bool read(istream_linker& file) {
		return file.read(ShadowMapChannel, DepthMap);
	}
};

struct FReflectionCaptureMapBuildData {
	int32 CubemapSize;
	float AverageBrightness;
	float Brightness;
	TArray<uint8>	FullHDRCapturedData;
	TArray<uint8>	EncodedHDRCapturedData;
	size_t			AllocatedSize;

	bool read(istream_linker& file) {
		return file.read(CubemapSize, AverageBrightness, Brightness, FullHDRCapturedData, EncodedHDRCapturedData);
	}
};

struct FSkyAtmosphereMapBuildData {
	bool bDummy = false;
};

template<int32 Order> struct TSHVector {
	enum { MaxSHBasis = Order * Order };
	float V[MaxSHBasis];
};

template<int32 MaxSHOrder> struct TSHVectorRGB {
public:
	TSHVector<MaxSHOrder> R;
	TSHVector<MaxSHOrder> G;
	TSHVector<MaxSHOrder> B;
};

template<int32 SHOrder> struct TVolumeLightingSample {
	FVector					Position;
	float					Radius;
	TSHVectorRGB<SHOrder>	Lighting;
	FColor					PackedSkyBentNormal;
	float					DirectionalLightShadowing;

	bool read(istream_linker& file){
		return file.read(Position, Radius, Lighting, PackedSkyBentNormal, DirectionalLightShadowing);
	}
};

typedef TVolumeLightingSample<3>	FVolumeLightingSample;
typedef TVolumeLightingSample<2>	FVolumeLightingSample2Band;

class FPrecomputedLightVolumeData {
public:
	FBox Bounds;
	float							SampleSpacing;
	int32							NumSHSamples;
	TArray<FVolumeLightingSample>	HighQualitySamples;
	TArray<FVolumeLightingSample>	LowQualitySamples;

	ISO_ptr<void>	HighQualityLightmap;
	ISO_ptr<void>	LowQualityLightmap;

	bool read(istream_linker& file) {
		if (file.get<bool32>() && file.get<bool32>()) {
			file.read(Bounds);
			SampleSpacing	= file.get();
			NumSHSamples	= file.get();
			file.read(HighQualitySamples);
			file.read(LowQualitySamples);


		}
		return true;
	}
};



struct UMapBuildDataRegistry : UObject {
	TMap<FGuid, FMeshMapBuildData>					 MeshBuildData;
	TMap<FGuid, TKnownPtr<FPrecomputedLightVolumeData>>			LevelPrecomputedLightVolumeBuildData;
	TMap<FGuid, TKnownPtr<FPrecomputedVolumetricLightmapData>>	LevelPrecomputedVolumetricLightmapBuildData;
	TMap<FGuid, FLightComponentMapBuildData>		 LightBuildData;
	TMap<FGuid, FReflectionCaptureMapBuildData>		 ReflectionCaptureBuildData;
	TMap<FGuid, FSkyAtmosphereMapBuildData>			 SkyAtmosphereBuildData;

	bool read(istream_linker& file) {
		UObject::read(file);
		auto strip = file.get<FStripDataFlags>();
		return strip.IsDataStrippedForServer()
			|| file.read(MeshBuildData, LevelPrecomputedLightVolumeBuildData, LevelPrecomputedVolumetricLightmapBuildData, LightBuildData, ReflectionCaptureBuildData, SkyAtmosphereBuildData);
	}
};

struct FURL {
	FString			Protocol;
	FString			Host;
	int32			Port;
	int32			Valid;
	FString			Map;
	TArray<FString>	Op;
	FString			Portal;

	bool	read(istream_ref file) {
		return file.read(Protocol, Host, Map, Portal, Op, Port, Valid);
	}
};

struct ULevel : UObject {
	TArray<TPtr<UObject>>			Actors;
	FURL							URL;
	TPtr<UObject>					Model;
	TPtr<UObject>					ModelComponents;
	TPtr<UObject>					LevelScriptBlueprint;
	TPtr<UObject>					NavListStart;
	TPtr<UObject>					NavListEnd;
	FPrecomputedVisibilityHandler	PrecomputedVisibilityHandler;
	FPrecomputedVolumeDistanceField	PrecomputedVolumeDistanceField;

	bool	read(istream_linker& file) {
		return UObject::read(file)
			&& file.read(Actors, URL, Model, ModelComponents, LevelScriptBlueprint, NavListStart, NavListEnd)
			;//&& iso::read(PrecomputedVisibilityHandler, PrecomputedVolumeDistanceField);
	}
};

//


template<int32 Order> ISO_DEFUSERCOMPVT(TSHVector, Order, V);
template<int32 Order> ISO_DEFUSERCOMPVT(TSHVectorRGB, Order, R, G, B);
template<int32 Order> ISO_DEFUSERCOMPVT(TVolumeLightingSample, Order, Position, Radius, Lighting, PackedSkyBentNormal, DirectionalLightShadowing);


ISO_DEFUSERCOMPV(FPerInstanceLightmapData, LightmapUVBias, ShadowmapUVBias);
ISO_DEFUSERCOMPV(FStaticShadowDepthMapData, WorldToLight, ShadowMapSizeX, ShadowMapSizeY, DepthSamples);

ISO_DEFUSERCOMPV(FPrecomputedLightVolumeData, Bounds, SampleSpacing, NumSHSamples, HighQualitySamples, LowQualitySamples);
ISO_DEFUSERCOMPV(FVolumetricLightmapDataLayer, Data, PixelFormatString);
ISO_DEFUSERCOMPV(FVolumetricLightmapBrickData, AmbientVector, SHCoefficients, SkyBentNormal, DirectionalLightShadowing, LQLightColor, LQLightDirection);
ISO_DEFUSERCOMPV(FPrecomputedVolumetricLightmapData,
	Bounds, IndirectionTextureDimensions, IndirectionTexture, BrickSize, BrickDataDimensions, BrickData, SubLevelBrickPositions, IndirectionTextureOriginalValues,
	BrickDataBaseOffsetInAtlas, SceneDataAdded, CPUSubLevelIndirectionTable, CPUSubLevelBrickDataList, IndexInCPUSubLevelBrickDataList
);

ISO_DEFUSERCOMPV(FMeshMapBuildData, LightMap, ShadowMap, IrrelevantLights, PerInstanceLightmapData);
ISO_DEFUSERCOMPV(FLightComponentMapBuildData, ShadowMapChannel, DepthMap);
ISO_DEFUSERCOMPV(FSkyAtmosphereMapBuildData, bDummy);
ISO_DEFUSERCOMPV(FReflectionCaptureMapBuildData, CubemapSize, AverageBrightness, Brightness, FullHDRCapturedData, EncodedHDRCapturedData, AllocatedSize);

ISO_DEFUSERCOMPV(UMapBuildDataRegistry, MeshBuildData, LevelPrecomputedLightVolumeBuildData, LevelPrecomputedVolumetricLightmapBuildData, LightBuildData, ReflectionCaptureBuildData, SkyAtmosphereBuildData);
ISO_DEFUSERCOMPV(FLightMap, LightGuids);
ISO_DEFUSER(FLegacyLightMap1D, FLightMap);
ISO_DEFUSERCOMPBV(FLightMap2D, FLightMap, Textures, SkyOcclusionTexture, AOMaterialMaskTexture, ShadowMapTexture, VirtualTexture, ScaleAddVectors, CoordinateScale, CoordinateBias, InvUniformPenumbraSize, bShadowChannelValid);
ISO_DEFUSERCOMPV(FShadowMap, LightGuids);
ISO_DEFUSERCOMPBV(FShadowMap2D, FShadowMap, Texture, CoordinateScale, CoordinateBias, bChannelValid, InvUniformPenumbraSize);

//
ISO_DEFUSERCOMPV(FPrecomputedVisibilityCell, Min, ChunkIndex, DataOffset);
ISO_DEFUSERCOMPV(FCompressedVisibilityChunk, bCompressed, UncompressedSize, Data);
ISO_DEFUSERCOMPV(FPrecomputedVisibilityBucket, CellDataSize, Cells, CellDataChunks);
ISO_DEFUSERCOMPV(FPrecomputedVisibilityHandler, CellBucketOriginXY, CellSizeXY, CellSizeZ, CellBucketSizeXY, NumCellBuckets, CellBuckets);
ISO_DEFUSERCOMPV(FPrecomputedVolumeDistanceField, VolumeMaxDistance, VolumeBox, VolumeSizeX, VolumeSizeY, VolumeSizeZ, Data);
ISO_DEFUSERCOMPBV(ULevel, UObject, Actors, URL, Model, ModelComponents, LevelScriptBlueprint, NavListStart, NavListEnd, PrecomputedVisibilityHandler, PrecomputedVolumeDistanceField);

ObjectLoaderRaw<UMapBuildDataRegistry>	load_MapBuildDataRegistry("MapBuildDataRegistry");
ObjectLoaderRaw<ULevel>					load_Level("Level");

//-----------------------------------------------------------------------------
//	function
//-----------------------------------------------------------------------------

enum EFunctionFlags : uint32 {
	FUNC_None				= 0x00000000,

	FUNC_Final				= 0x00000001,	// Function is final (prebindable, non-overridable function)
	FUNC_RequiredAPI		= 0x00000002,	// Indicates this function is DLL exported/imported
	FUNC_BlueprintAuthorityOnly= 0x00000004,   // Function will only run if the object has network authority
	FUNC_BlueprintCosmetic	= 0x00000008,   // Function is cosmetic in nature and should not be invoked on dedicated servers
	FUNC_Net				= 0x00000040,   // Function is network-replicated
	FUNC_NetReliable		= 0x00000080,   // Function should be sent reliably on the network
	FUNC_NetRequest			= 0x00000100,	// Function is sent to a net service
	FUNC_Exec				= 0x00000200,	// Executable from command line
	FUNC_Native				= 0x00000400,	// Native function
	FUNC_Event				= 0x00000800,   // Event function
	FUNC_NetResponse		= 0x00001000,   // Function response from a net service
	FUNC_Static				= 0x00002000,   // Static function
	FUNC_NetMulticast		= 0x00004000,	// Function is networked multicast Server -> All Clients
	FUNC_UbergraphFunction	= 0x00008000,   // Function is used as the merge 'ubergraph' for a blueprint, only assigned when using the persistent 'ubergraph' frame
	FUNC_MulticastDelegate	= 0x00010000,	// Function is a multi-cast delegate signature (also requires FUNC_Delegate to be set!)
	FUNC_Public				= 0x00020000,	// Function is accessible in all classes (if overridden, parameters must remain unchanged)
	FUNC_Private			= 0x00040000,	// Function is accessible only in the class it is defined in (cannot be overridden, but function name may be reused in subclasses.  IOW: if overridden, parameters don't need to match, and Super.Func() cannot be accessed since it's private.)
	FUNC_Protected			= 0x00080000,	// Function is accessible only in the class it is defined in and subclasses (if overridden, parameters much remain unchanged)
	FUNC_Delegate			= 0x00100000,	// Function is delegate signature (either single-cast or multi-cast, depending on whether FUNC_MulticastDelegate is set.)
	FUNC_NetServer			= 0x00200000,	// Function is executed on servers (set by replication code if passes check)
	FUNC_HasOutParms		= 0x00400000,	// function has out (pass by reference) parameters
	FUNC_HasDefaults		= 0x00800000,	// function has structs that contain defaults
	FUNC_NetClient			= 0x01000000,	// function is executed on clients
	FUNC_DLLImport			= 0x02000000,	// function is imported from a DLL
	FUNC_BlueprintCallable	= 0x04000000,	// function can be called from blueprint code
	FUNC_BlueprintEvent		= 0x08000000,	// function can be overridden/implemented from a blueprint
	FUNC_BlueprintPure		= 0x10000000,	// function can be called from blueprint code, and is also pure (produces no side effects). If you set this, you should set FUNC_BlueprintCallable as well
	FUNC_EditorOnly			= 0x20000000,	// function can only be called from an editor scrippt
	FUNC_Const				= 0x40000000,	// function can be called from blueprint code, and only reads state (never writes state)
	FUNC_NetValidate		= 0x80000000,	// function must supply a _Validate implementation

	FUNC_AllFlags			= 0xFFFFFFFF,

	FUNC_FuncInherit		= FUNC_Exec | FUNC_Event | FUNC_BlueprintCallable | FUNC_BlueprintEvent | FUNC_BlueprintAuthorityOnly | FUNC_BlueprintCosmetic | FUNC_Const,
	FUNC_FuncOverrideMatch	= FUNC_Exec | FUNC_Final | FUNC_Static | FUNC_Public | FUNC_Protected | FUNC_Private,
	FUNC_NetFuncFlags		= FUNC_Net | FUNC_NetReliable | FUNC_NetServer | FUNC_NetClient | FUNC_NetMulticast,
	FUNC_AccessSpecifiers	= FUNC_Public | FUNC_Private | FUNC_Protected,
};

struct UFunction : UStruct {
	EFunctionFlags	FunctionFlags;
	uint16			RPCId;
	uint16			RPCResponseId;
	TPtr<FProperty>	FirstPropertyToInit;
	TPtr<UFunction>	EventGraphFunction;
	int32			EventGraphCallOffset;

	bool	read(istream_linker& file) {
		UStruct::read(file);
		file.read(FunctionFlags);

		if (FunctionFlags & FUNC_Net) {
			file.get<int16>();
		}

		file.read(EventGraphFunction);
		file.read(EventGraphCallOffset);
		return true;
	}
};

ISO_DEFUSER(EFunctionFlags, uint32);
ISO_DEFUSERCOMPBV(UFunction, UStruct, FunctionFlags, RPCId, RPCResponseId, FirstPropertyToInit, EventGraphFunction, EventGraphCallOffset);
ObjectLoaderRaw<UFunction>	load_Function("Function");

//-----------------------------------------------------------------------------
//	material
//-----------------------------------------------------------------------------

struct FExpressionInput {
//	TPtr<UMaterialExpression>	Expression;
	int32	OutputIndex;
	FName	InputName;
	int32	Mask, MaskR, MaskG, MaskB, MaskA;
	FName	ExpressionName;

	FExpressionInput() : OutputIndex(0), Mask(0), MaskR(0), MaskG(0), MaskB(0), MaskA(0) {}

	bool read(istream_linker& file) {
		return file.read(OutputIndex, InputName, Mask, MaskR, MaskG, MaskB, MaskA);
	}
};
ISO_DEFUSERCOMPV(FExpressionInput, OutputIndex, InputName, Mask, MaskR, MaskG, MaskB, MaskA);
ObjectLoaderRaw<FExpressionInput>	load_ExpressionInput("ExpressionInput"),
									load_MaterialAttributesInput("MaterialAttributesInput");

template<typename T> struct FMaterialInput : FExpressionInput {
	bool	bUseConstant;
	T		Constant;

	bool read(istream_linker& file) {
		if (!FExpressionInput::read(file))
			return false;
		bUseConstant = file.get<bool32>();
		return file.read(Constant);
	}
};

template<typename T> ISO_DEFUSERCOMPBVT(FMaterialInput, T, FExpressionInput, bUseConstant, Constant);

ObjectLoaderRaw<FMaterialInput<float>>		load_ScalarMaterialInput("ScalarMaterialInput");
ObjectLoaderRaw<FMaterialInput<FColor>>		load_ColorMaterialInput("ColorMaterialInput");
ObjectLoaderRaw<FMaterialInput<uint32>>		load_ShadingModelMaterialInput("ShadingModelMaterialInput");
ObjectLoaderRaw<FMaterialInput<FVector>>	load_VectorMaterialInput("VectorMaterialInput");
ObjectLoaderRaw<FMaterialInput<FVector2D>>	load_Vector2MaterialInput("Vector2MaterialInput");

//-----------------------------------------------------------------------------
//	spline
//-----------------------------------------------------------------------------

struct FRichCurveKey {
	enum ERichCurveInterpMode			: uint8 { RCIM_Linear, RCIM_Constant, RCIM_Cubic, RCIM_None };
	enum ERichCurveTangentMode			: uint8 { RCTM_Auto, RCTM_User, RCTM_Break, RCTM_None };
	enum ERichCurveTangentWeightMode	: uint8 { RCTWM_WeightedNone, RCTWM_WeightedArrive, RCTWM_WeightedLeave, RCTWM_WeightedBoth };

	uint8/*ERichCurveInterpMode			*/InterpMode;
	uint8/*ERichCurveTangentMode		*/TangentMode;
	uint8/*ERichCurveTangentWeightMode	*/TangentWeightMode;
	float						Time;
	float						Value;
	float						ArriveTangent;
	float						ArriveTangentWeight;
	float						LeaveTangent;
	float						LeaveTangentWeight;

	bool read(istream_linker& file) {
		return file.read(InterpMode, TangentMode, TangentWeightMode, Time, Value, ArriveTangent, ArriveTangentWeight, LeaveTangent, LeaveTangentWeight);
	}
};

ObjectLoaderRaw<FRichCurveKey>		load_RichCurveKey("RichCurveKey");
ISO_DEFUSERCOMPV(FRichCurveKey, InterpMode, TangentMode, TangentWeightMode, Time, Value, ArriveTangent, ArriveTangentWeight, LeaveTangent, LeaveTangentWeight);

//-----------------------------------------------------------------------------
//	FAssetRegistry
//-----------------------------------------------------------------------------

//struct istream_namemap : reader<istream_namemap>, reader_ref<istream_ref> {
//	TArray<FNameEntrySerialized> NameMap;
//
//	istream_namemap(istream_ref file) : reader_ref<istream_ref>(file) {
//		auto	TotalSize = file.length();
//		int64	NameOffset = file.get<int64>();
//		if (NameOffset < TotalSize && NameOffset > 0)
//			make_save_pos(file, NameOffset), NameMap.read(file);
//	}
//	cstring	lookup(const FName& name) const {
//		if (name.ComparisonIndex < NameMap.size())
//			return NameMap[name.ComparisonIndex].AnsiName;
//		ISO_TRACE("Bad name\n");
//		return "None";
//	}
//};

struct FAssetData {
	FName2			ObjectPath;
	FName2			PackageName;
	FName2			PackagePath;
	FName2			AssetName;
	FName2			AssetClass;
	TMap<FName2, FString> TagsAndValues;
	TArray<int32>	ChunkIDs;
	uint32			PackageFlags;

	bool read(istream_linker &file) {
		return file.read(ObjectPath, PackagePath, AssetClass, PackageName, AssetName, TagsAndValues, ChunkIDs, PackageFlags);
	}
};

struct FAssetIdentifier {
	FName	PackageName;
	FName	PrimaryAssetType;
	FName	ObjectName;
	FName	ValueName;

	bool read(istream_ref file) {
		uint8 FieldBits = file.getc();
		if (FieldBits & (1 << 0))
			file.read(PackageName);
		if (FieldBits & (1 << 1))
			file.read(PrimaryAssetType);
		if (FieldBits & (1 << 2))
			file.read(ObjectName);
		if (FieldBits & (1 << 3))
			file.read(ValueName);

		return true;
	}
};

struct FDependsNode {
	typedef TArray<FDependsNode*> FDependsNodeList;
	FAssetIdentifier Identifier;
	FDependsNodeList HardDependencies;
	FDependsNodeList SoftDependencies;
	FDependsNodeList NameDependencies;
	FDependsNodeList SoftManageDependencies;
	FDependsNodeList HardManageDependencies;
	FDependsNodeList Referencers;

	bool read(istream_ref file, const TArray<FDependsNode> &DependsNodeDataBuffer) {
		file.read(Identifier);

		int32	num_hard, num_soft, num_name, num_softmanage, num_hardmanage, num_ref;
		file.read(num_hard, num_soft, num_name, num_softmanage, num_hardmanage, num_ref);

		auto SerializeDependencies = [&](FDependsNodeList &list, int num) {
			list.reserve(num);
			for (int32 DependencyIndex = 0; DependencyIndex < num; ++DependencyIndex) {
				int32 Index = file.get<int32>();
				if (Index < 0 || Index >= DependsNodeDataBuffer.size())
					return false;

				list.push_back(&DependsNodeDataBuffer[Index]);
			}
			return true;
		};

		return SerializeDependencies(HardDependencies, num_hard)
			&& SerializeDependencies(SoftDependencies, num_soft)
			&& SerializeDependencies(NameDependencies, num_name)
			&& SerializeDependencies(SoftManageDependencies, num_softmanage)
			&& SerializeDependencies(HardManageDependencies, num_hardmanage)
			&& SerializeDependencies(Referencers, num_ref);
	}

};

typedef uint8  FMD5Hash[16];

struct FAssetPackageData {
	int64		DiskSize;
	FGuid		PackageGuid;
	FMD5Hash	CookedHash;
	bool read(istream_ref file) {
		return file.read(DiskSize, PackageGuid, CookedHash);
	}
};

struct FAssetRegistryState : FLinkerTables {
	enum EVersion {
		PreVersioning = 0,		// From before file versioning was implemented
		HardSoftDependencies,	// The first version of the runtime asset registry to include file versioning
		AddAssetRegistryState,	// Added FAssetRegistryState and support for piecemeal serialization
		ChangedAssetData,		// AssetData serialization format changed, versions before this are not readable
		RemovedMD5Hash,			// Removed MD5 hash from package data
		AddedHardManage,		// Added hard/soft manage references
		AddedCookedMD5Hash,		// Added MD5 hash of cooked package to package data

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	static const FGuid GUID;

	TArray<FAssetData>			AssetDataBuffer;
	TArray<FDependsNode>		DependsNodeDataBuffer;
	TArray<FAssetPackageData>	PackageDataBuffer;

	bool read(istream_ref file);
};

const FGuid FAssetRegistryState::GUID(0x717F9EE7, 0xE9B0493A, 0x88B39132, 0x1B388107);


bool FAssetRegistryState::read(istream_ref file) {
	FGuid		guid;
	EVersion	Version;
	if (!file.read(guid) || guid != GUID || !file.read(Version) || Version < RemovedMD5Hash)
		return false;

	auto	TotalSize = file.length();
	int64	NameOffset = file.get<int64>();
	if (NameOffset < TotalSize && NameOffset > 0)
		make_save_pos(file, NameOffset), read_names(file, file.get<int32>(), true);

	istream_linker	file2(file, file, this);

	AssetDataBuffer.resize(file2.get<int32>());
	for (auto &i : AssetDataBuffer)
		file2.read(i);

	DependsNodeDataBuffer.resize(file2.get<int32>());
	for (auto &Depends : DependsNodeDataBuffer) {
		if (!Depends.read(file2, DependsNodeDataBuffer))
			return false;
	}

	PackageDataBuffer.resize(file2.get<int32>());
	for (auto &Package : PackageDataBuffer) {
		FName PackageName;
		file2.read(PackageName);

		if (Version < AddedCookedMD5Hash) {
			file2.read(Package.DiskSize);
			file2.read(Package.PackageGuid);
		} else {
			Package.read(file2);
		}
	}
	return true;
}

ISO_DEFUSERCOMPV(FAssetData, ObjectPath, PackageName, PackagePath, AssetName, AssetClass, TagsAndValues, ChunkIDs, PackageFlags);
ISO_DEFUSERCOMPV(FAssetIdentifier, PackageName, PrimaryAssetType, ObjectName, ValueName);
ISO_DEFUSERCOMPV(FDependsNode, Identifier, HardDependencies, SoftDependencies, NameDependencies, SoftManageDependencies, HardManageDependencies, Referencers);
ISO_DEFUSERCOMPV(FAssetPackageData, DiskSize, PackageGuid, CookedHash);
ISO_DEFUSERCOMPV(FAssetRegistryState, AssetDataBuffer, DependsNodeDataBuffer, PackageDataBuffer);

//-----------------------------------------------------------------------------
//	common
//-----------------------------------------------------------------------------

template<typename T> _ISO_DEFSAME(TPtr<T>, ISO_ptr<void>);
template<typename T> _ISO_DEFSAME(TInlinePtr<T>, ISO_ptr<void>);
template<typename T> _ISO_DEFSAME(TKnownPtr<T>, ISO_ptr<T>);
template<typename T> _ISO_DEFSAME(TSoftPtr<T>, FSoftObjectPath);
template<typename T> _ISO_DEFSAME(TArray<T>, dynamic_array<T>);
template<typename T> _ISO_DEFSAME(TBulkArray<T>, dynamic_array<T>);
template<typename T> _ISO_DEFSAME(TBulkArrayData<T>, dynamic_array<T>);
template<typename T> _ISO_DEFSAME(TSparseArray<T>, sparse_array<T COMMA uint32 COMMA uint32>);
template<typename K, typename V> _ISO_DEFSAME(TMap<K COMMA V>, map<K COMMA V>);
template<typename K> _ISO_DEFSAME(TSet<K>, set<K>);

template<> struct ISO::def<TBitArray>				: ISO::VirtualT2<TBitArray>		{
	static uint32	Count(TBitArray& a)				{ return a.size32(); }
	static ISO::Browser2 Index(TBitArray& a, int i)	{ return MakeBrowser(bool(a[i])); }
};

ISO_DEFUSER(FString, char*);
ISO_DEFUSER(bool32, bool);
ISO_DEFUSER(EObjectFlags,xint32);
ISO_DEFUSER(FName2, char*);

ISO_DEFUSERCOMPV(FVector2D, X, Y);
ISO_DEFUSERCOMPV(FVector, X, Y, Z);
ISO_DEFUSERCOMPV(FVector4, X, Y, Z, W);
ISO_DEFUSERCOMPV(FQuat, X, Y, Z, W);
ISO_DEFUSERCOMPV(FIntPoint, X, Y);
ISO_DEFUSERCOMPV(FIntVector, X, Y, Z);
ISO_DEFUSERCOMPV(FMatrix, X, Y, Z, W);
ISO_DEFUSERCOMPV(FTransform, Rotation, Translation, Scale3D);
ISO_DEFUSERCOMPV(FColor, B,G,R,A);
ISO_DEFUSERCOMPV(FLinearColor, R,G,B,A);
ISO_DEFUSERCOMPV(FRotator, Pitch, Yaw, Roll);
ISO_DEFUSERCOMPV(FBox, Min, Max, IsValid);
ISO_DEFUSERCOMPV(FSphere, Center, W);
ISO_DEFUSERCOMPV(FBoxSphereBounds, Origin, BoxExtent, SphereRadius);
ISO_DEFUSERCOMPV(FGuid, A, B, C, D);
ISO_DEFUSERCOMPV(FName, ComparisonIndex, Number);
ISO_DEFUSERCOMPV(FSoftObjectPath, AssetPathName, SubPathString);
ISO_DEFSAME(FSoftClassPath, FSoftObjectPath);
ISO_DEFUSERCOMPV(FURL, Protocol, Host, Port, Valid, Map, Op, Portal);

void LoadThumbnails(istream_ref file, anything &thumbs) {
	int32 n = file.get<int>();
	for (int32 i = 0; i < n; ++i) {
		FString	ObjectClassName					= file.get();
		FString	ObjectPathWithoutPackageName	= file.get();
		int32	FileOffset						= file.get();

		if (ObjectClassName && ObjectClassName != "???"_cstr) {
			auto		fp = make_save_pos(file, FileOffset);
			int32		width, height, size;
			read(file, width, height, size);
			if (height > 0)
				thumbs.Append(FileHandler::Get("png")->Read((const char*)ObjectClassName, file));
			else
				thumbs.Append(FileHandler::Get("jpg")->Read((const char*)ObjectClassName, file));
		}
	}
}

} // namespace unreal

using namespace unreal;

struct UnrealAsset : anything {};
ISO_DEFUSER(UnrealAsset,anything);

ISO_ptr<void> ExtractUnrealAsset(ISO_ptr<UnrealAsset> p) {
	for (ISO::Browser2 i : *p) {
		if (i.IsPtr())
			i = *i;
		if (i.Is<USoundWave>())
			return ((USoundWave*)i)->Sample;
		if (i.Is<UTexture2D>())
			return ((UTexture2D*)i)->Bitmap;
		if (i.Is<UStaticMesh>())
			return ((UStaticMesh*)i)->Model;
		if (i.Is<USkeletalMesh>())
			return ((USkeletalMesh*)i)->Model;
	}
	return ISO_NULL;
}
  
//-----------------------------------------------------------------------------
// FileHandler
//-----------------------------------------------------------------------------

ISO_ptr<void> ReadSerialised(istream_linker& file, const FObjectExport &exp) {
	ISO_ptr<void>	p;

	if (auto Class = file.lookup(exp.ClassIndex)) {
		auto	class_name = file.lookup(Class->ObjectName);
		if (auto i = ObjectLoader::get(class_name)) {
			p = i->load(file);

		} else if (auto type = ISO::user_types.Find("U" + class_name)) {
			p = ISO::MakePtr(type);
			ReadTagged(file, ISO::Browser2(p));
			if (file.get<bool32>())
				((UObject*)p)->Append(ISO::MakePtr("guid", file.get<FGuid>()));

		} else {
			ISO_TRACEFI("\tLoading class %0\n", class_name);
			p = load_Object.load(file);
			p.SetID(class_name);
		}
	} else {
		p = load_Object.load(file);
	}
	return p;
}

ISO_ptr<void> ReadPackage(tag id, istream_ref file, istream_ref bulk_file) {
	FPackageFileSummary	sum;
	if (!file.read(sum))
		return ISO_NULL;

	ISO_ptr<UnrealAsset>	p(id);


	ISO_ptr<anything>	thumbs("thumbnails");
	p->Append(thumbs);
	file.seek(sum.ThumbnailTableOffset);
	LoadThumbnails(file, *thumbs);

	FLinkerTables	linker_tables(file, sum);

	for (auto& i : linker_tables.ExportMap)
		i.p = ISO::MakePtr<ISO_ptr<void>>(linker_tables.lookup(i.ObjectName));

	for (auto& i : linker_tables.ImportMap)
		i.p = ISO::MakePtrExternal<ISO_ptr<void>>("C:\\temp", linker_tables.lookup(i.ObjectName));

	istream_linker	file2(file, bulk_file, &linker_tables);
	for (auto& i : linker_tables.ExportMap) {
		ISO_TRACEF("Load ") << linker_tables.lookup(i.ObjectName) << '\n';

		if (i.SerialSize) {
			file.seek(i.SerialOffset);
			*i.p = ReadSerialised(file2, i);
			p->Append(i.p);
			if (auto remain = (i.SerialOffset + i.SerialSize) - file.tell())
				ISO_TRACEF("\tRemaining: ") << remain << '\n';
		}
	}

	if (sum.AssetRegistryDataOffset > 0) {
		file.seek(sum.AssetRegistryDataOffset);

		ISO_ptr<anything>	reg("Registry");
		p->Append(reg);

		int64	OutDependencyDataOffset = -1;

		if (sum.FileVersionUE4 >= VER_UE4_ASSETREGISTRY_DEPENDENCYFLAGS)// && !(PackageFileSummary.GetPackageFlags() & PKG_FilterEditorOnly);
			file.read(OutDependencyDataOffset);

		int32 n = file.get<int>();
		for (int32 i = 0; i < n; ++i) {
			FString ObjectPath = file.get();
			FString ObjectClassName = file.get();
			ISO_ptr<anything>	p2((const char*)ObjectPath);
			reg->Append(p2);

			p2->Append(ISO::MakePtr("Class", ObjectClassName));
			p2->Append(ISO::MakePtr("Tags", file.get<TMap<FString, FString>>()));
			int32	TagCount = file.get<int>();
		}
	}
	return p;
}

class UnrealFileHandler : public FileHandler {
	const char* GetExt()			override { return "uasset"; }
	const char* GetDescription()	override { return "Unreal asset"; }

	int Check(istream_ref file) override {
		file.seek(0);
		return is_any(file.get<uint32>(), FPackageFileSummary::PACKAGE_FILE_TAG,  swap_endian(FPackageFileSummary::PACKAGE_FILE_TAG)) ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}


	ISO_ptr<void> Read(tag id, istream_ref file) override {
		return ReadPackage(id, file, file);
	}

	ISO_ptr<void> ReadWithFilename(tag id, const filename& fn) override {
		filename	uexp = filename(fn).set_ext("uexp");
		if (uexp.exists()) {
			filename	ubulk = filename(fn).set_ext("ubulk");
			if (ubulk.exists())
				return ReadPackage(id, make_combined_reader(FileInput(fn), FileInput(uexp)), FileInput(ubulk));
			return Read(id, make_combined_reader(FileInput(fn), FileInput(uexp)));
		}
		return Read(id, FileInput(fn));
	}

public:
	UnrealFileHandler() {
		ISO::getdef<UNodeMappingContainer>();
		ISO_get_conversion(ExtractUnrealAsset);
	}
} unreal_filehandler;

//-----------------------------------------------------------------------------
// SaveGame
//-----------------------------------------------------------------------------

struct FSaveGameHeader {
	static const int UE4_SAVEGAME_FILE_TYPE_TAG = 0x53415647;		// "sAvG"
	enum Type {
		InitialVersion = 1,
		AddedCustomVersions = 2,

		VersionPlusOne,
		LatestVersion = VersionPlusOne - 1
	};

	int32 FileTypeTag;
	int32 SaveGameFileVersion;
	int32 PackageFileUE4Version;
	FEngineVersion SavedEngineVersion;
	int32 CustomVersionFormat;
	FCustomVersionContainer CustomVersions;
	FString SaveGameClassName;

	bool	read(istream_ref file) {
		file.read(FileTypeTag);

		if (FileTypeTag != UE4_SAVEGAME_FILE_TYPE_TAG) {
			file.seek(0);
			SaveGameFileVersion = InitialVersion;
		} else {
			file.read(SaveGameFileVersion, PackageFileUE4Version, SavedEngineVersion);

			if (SaveGameFileVersion >= AddedCustomVersions) {
				file.read(CustomVersionFormat);
				CustomVersions.read(file, static_cast<FCustomVersionContainer::Type>(CustomVersionFormat));
			}
		}

		file.read(SaveGameClassName);
		return true;
	}
};

struct WESaveGameHeader {
	static const uint32	TAG = 'WESG';
	uint32					FileTypeTag;
	int32					PackageFileUE4Version;
	FEngineVersion			SavedEngineVersion;
	int32					CustomVersionFormat;
	FCustomVersionContainer CustomVersions;

	bool	read(istream_ref file) {
		file.read(FileTypeTag, PackageFileUE4Version, SavedEngineVersion, CustomVersionFormat);
		CustomVersions.read(file, static_cast<FCustomVersionContainer::Type>(CustomVersionFormat));
		return FileTypeTag == TAG;
	}
};

class UnrealSaveGameFileHandler : public FileHandler {

	const char* GetExt() override { return "sav"; }
	const char* GetDescription() override { return "Unreal saved game"; }

	int Check(istream_ref file) override {
		file.seek(0);
		return is_any(file.get<uint32>(), FSaveGameHeader::UE4_SAVEGAME_FILE_TYPE_TAG, WESaveGameHeader::TAG) ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void> Read(tag id, istream_ref file) override {
		FLinkerTables	tables;
		istream_linker	linker(file, file, &tables);

		WESaveGameHeader	header1;
		if (header1.read(file)) {
			ISO_ptr<anything>	p(id);

			FString	ActorName;
			while (file.read(ActorName)) {
				ISO_ptr<anything>	p2((const char*)ActorName);
				p->Append(p2);

				for (FPropertyTag prop; prop.read(linker);)
					p2->Append(ReadTag(linker, prop, linker.lookup(prop.Name)));

				file.seek_cur(4);
			}
			return p;
		}

		FSaveGameHeader	header;
		file.seek(0);
		if (!header.read(file))
			return ISO_NULL;

		ISO_ptr<anything>	p(id);
		for (FPropertyTag prop; prop.read(linker);) {
			p->Append(ReadTag(linker, prop, linker.lookup(prop.Name)));
		}
		
		return p;


	}
} unreal_sav;


//-----------------------------------------------------------------------------
// PAK
//-----------------------------------------------------------------------------

struct FSHAHash {
	uint8 Hash[20];
};

struct FPakInfo {
	enum {
		PakFile_Magic						= 0x5A6F12E1,
		MaxChunkDataSize					= 64*1024,
		CompressionMethodNameLen			= 32,
		MaxNumCompressionMethods			= 5,

		Version_Initial						= 1,
		Version_NoTimestamps				= 2,
		Version_CompressionEncryption		= 3,
		Version_IndexEncryption				= 4,
		Version_RelativeChunkOffsets		= 5,
		Version_DeleteRecords				= 6,
		Version_EncryptionKeyGuid			= 7,
		Version_FNameBasedCompressionMethod	= 8,
		Version_FrozenIndex					= 9,

		Version_Last,
		Version_Invalid,
		Version_Latest						= Version_Last - 1
	};

	uint32			Magic;
	int32			Version;
	int64			IndexOffset;
	int64			IndexSize;
	FSHAHash		IndexHash;
	uint8			bEncryptedIndex;
	uint8			bIndexIsFrozen;
	FGuid			EncryptionKeyGuid;
	TArray<FString>	CompressionMethods;

	static size_t	serialized_size(uint32 ver) {
		size_t size = sizeof(Magic) + sizeof(Version) + sizeof(IndexOffset) + sizeof(IndexSize) + sizeof(IndexHash) + sizeof(bEncryptedIndex);
//		if (ver >= Version_EncryptionKeyGuid)
//			size += sizeof(EncryptionKeyGuid);
		if (ver >= Version_FNameBasedCompressionMethod)
			size += CompressionMethodNameLen * MaxNumCompressionMethods;
		if (ver >= Version_FrozenIndex)
			size += sizeof(bIndexIsFrozen);
		return size;
	}
	bool	valid() const {
		return Magic == PakFile_Magic;
	}
	bool	read(istream_ref file, uint32 ver) {
		file.read(bEncryptedIndex, Magic, Version, IndexOffset, IndexSize, IndexHash);
		if (Magic != PakFile_Magic)
			return false;

		if (ver < Version_IndexEncryption)
			bEncryptedIndex = false;

		if (ver < Version_EncryptionKeyGuid)
			clear(EncryptionKeyGuid);

		if (ver >= Version_FrozenIndex)
			file.read(bIndexIsFrozen);

		if (ver < Version_FNameBasedCompressionMethod) {
			CompressionMethods.push_back("Zlib");
			CompressionMethods.push_back("Gzip");
			CompressionMethods.push_back("Oodle");
		} else {
			char	Methods[MaxNumCompressionMethods][CompressionMethodNameLen];
			file.read(Methods);
			for (auto i : Methods) {
				if (i[0] != 0)
					CompressionMethods.push_back(i);
			}
		}
		return true;
	}
};

struct FPakEntry {
	enum EFlags : uint8 {
		Flag_None		= 0x00,
		Flag_Encrypted	= 0x01,
		Flag_Deleted	= 0x02,
	};
	enum Compression : uint32 {
		COMPRESS_NONE	= 0,
		COMPRESS_ZLIB	= 1,
		COMPRESS_GZIP	= 2,
		COMPRESS_Custom	= 3,
	};
	struct FDateTime {
		int64	Ticks;	// 100 nanoseconds resolution since January 1, 0001 A.D
	};
	struct FPakCompressedBlock {
		int64	CompressedStart;
		int64	CompressedEnd;
	};

	int64		Offset;
	int64		Size;
	int64		UncompressedSize;
	FSHAHash	Hash;
	TArray<FPakCompressedBlock>	CompressionBlocks;
	uint32		CompressionBlockSize;
	Compression	CompressionMethod;
	EFlags		Flags;

	bool	read(istream_ref file, uint32 ver) {
		file.read(Offset, Size, UncompressedSize);

		if (ver < FPakInfo::Version_FNameBasedCompressionMethod) {
			enum ECompressionFlags : int32 {
				COMPFLAG_None						= 0x00,
				COMPFLAG_ZLIB						= 0x01,
				COMPFLAG_GZIP						= 0x02,
				COMPFLAG_Custom						= 0x04,
				COMPFLAG_DeprecatedFormatFlagsMask	= 0x0F,
				COMPFLAG_NoFlags					= 0x00,
				COMPFLAG_BiasMemory					= 0x10,
				COMPFLAG_BiasSpeed					= 0x20,
				COMPFLAG_SourceIsPadded				= 0x80,
				COMPFLAG_OptionsFlagsMask			= 0xF0,
			};
			ECompressionFlags Legacy = file.get<ECompressionFlags>();
			CompressionMethod
				= Legacy & COMPFLAG_ZLIB	? COMPRESS_ZLIB	
				: Legacy & COMPFLAG_GZIP	? COMPRESS_GZIP	
				: Legacy & COMPFLAG_Custom	? COMPRESS_Custom	
				: COMPRESS_NONE;
		} else {
			file.read(CompressionMethod);
		}

		if (ver <= FPakInfo::Version_Initial)
			file.get<FDateTime>();

		file.read(Hash);
		if (ver >= FPakInfo::Version_CompressionEncryption) {
			if (CompressionMethod)
				file.read(CompressionBlocks);
			file.read(Flags);
			file.read(CompressionBlockSize);
		}
		return true;
	}

	malloc_block	get(istream_ref file) const {
		malloc_block	data(UncompressedSize);
		switch (CompressionMethod) {
			case COMPRESS_NONE:
				file.seek(Offset);
				file.readbuff(data, UncompressedSize);
				break;

			case COMPRESS_ZLIB: {
				size_t	dst = 0;
				for (auto& i : CompressionBlocks) {
					file.seek(Offset + i.CompressedStart);
					transcode_from_file(zlib_decoder(), data + dst, file);
					dst += CompressionBlockSize;
				}
				break;
			}
			default:
				ISO_ASSERT(0);
				break;
		}
		return data;
	}
};

typedef TMap<FString, int32> FPakDirectory;

struct FPakFileData {
	FString						MountPoint;
	TArray<FPakEntry>			Files;
	TMap<FString, FPakDirectory>Index;
};

struct FPakFile {
	FPakInfo					info;
	unique_ptr<FPakFileData>	data;
	FString						MountPoint;
	bool	read(istream_ref file);
};

bool FPakFile::read(istream_ref file) {
	streamptr	total_size = file.length();
	for (uint32 ver = FPakInfo::Version_Latest; ver > FPakInfo::Version_Initial; --ver) {
		auto	info_pos	= total_size - info.serialized_size(ver);
		if (info_pos >= 0) {
			file.seek(info_pos);
			if (info.read(file, ver))
				break;
		}
	}
	if (!info.valid())
		return false;

	if (total_size < info.IndexOffset + info.IndexSize)
		return false;

	file.seek(info.IndexOffset);

	if (info.Version >= FPakInfo::Version_FrozenIndex && info.bIndexIsFrozen) {
		data		= malloc_block(file, info.IndexSize);
		MountPoint	= move(data->MountPoint);

	} else {
#if 0
		FSHAHash EncryptedDataHash;

		if (info.bEncryptedIndex)
			DecryptData(dataMemory, info.EncryptionKeyGuid);

		FSHAHash ComputedHash = FSHA1::HashBuffer(dataMemory, ComputedHash.Hash);

		if (info.IndexHash != ComputedHash)
			return;
#endif
		file.read(MountPoint);

		data.emplace();
		data->Files.resize(file.get<uint32>());

		uint64	entry_total	= 0;
		for (auto &Entry : data->Files) {
			int		EntryIndex	= data->Files.index_of(Entry);
			FString	Filename	= file.get<FString>();

			Entry.read(file, info.Version);

			entry_total	+= Entry.Size;

			filename	fn		= get(Filename);
			FString		path	= fn.dir().convert_to_fwdslash().begin();

			if (auto i = data->Index.find(path)) {
				FPakDirectory &Directory = *i;
				Directory.put(fn.name().begin(), EntryIndex);

			} else {
				FPakDirectory& Directory = data->Index.emplace(path);
				Directory.put(fn.name().begin(), EntryIndex);

				for (auto &i : with_iterator(parts<'/'>(get(path))))
					data->Index[i.full()];
			}
		}
		ISO_TRACEF("Missing ") << total_size - entry_total - info.IndexSize << '\n';
	}
	return true;
}

struct FPakFileRoot : refs<FPakFileRoot> {
	istream_ptr			file;
	ISO_ptr<Folder>		root;
	FAssetRegistryState	registry;
	FPakFileRoot(istream_ref file, const FPakFile& pak);
};
ISO_DEFUSERCOMPV(FPakFileRoot, root, registry);

struct FPakFileFile : ISO::VirtualDefaults {
	ref_ptr<FPakFileRoot>	r;
	FPakEntry			entry;
	const FAssetData	*asset;
	ISO_ptr<void>		cache;

	FPakFileFile(FPakFileRoot *r, const FPakEntry &entry) : r(r), entry(entry), asset(0) {}

//	uint32			Count() const { return entry.UncompressedSize; }
	ISO_ptr<void>	Deref()	{
		if (cache)
			return cache;

		if (!asset)
			return ISO::MakePtr(none, entry.get(r->file));

		cstring	class_name	= (const char*)asset->AssetClass;
#if 0
		if (class_name == "World") {
			memory_reader_owner	file0(entry.get(r->file));
			return cache = ReadPackage((const char*)asset->AssetName, file0, file0);
		}
#endif
		return ISO::MakePtr(none, entry.get(r->file));

		ISO_TRACEF("Load ") << asset->AssetName << '\n';

		memory_reader_owner	file0(entry.get(r->file));
		istream_linker		file(file0, file0, &r->registry);

		if (auto i = ObjectLoader::get(class_name))
			return i->load(file);

		if (auto type = ISO::user_types.Find("U" + class_name)) {
			ISO_ptr<void>	p = ISO::MakePtr(type);
			ReadTagged(file, ISO::Browser2(p));
			if (file.get<bool32>())
				((UObject*)p)->Append(ISO::MakePtr("guid", file.get<FGuid>()));

		}
		ISO_TRACEFI("\tLoading class %0\n", class_name);
		return load_Object.load(file);
	}
};
ISO_DEFUSERVIRTX(FPakFileFile, "File");

FPakFileRoot::FPakFileRoot(istream_ref	file, const FPakFile& pak) : file(file.clone()), root("root") {
	for (auto &i : pak.data->Index.with_keys()) {
		ISO_TRACEF("Making folder ") << i.a << '\n';
		auto	folder = GetDir(root, get(i.a));
		for (auto j : i.b.with_keys()) {
			ISO_TRACEF("  adding file ") << (const char*)j.a << '\n';
			auto	&entry = pak.data->Files[j.b];
			if (j.a == "AssetRegistry"_cstr && false) {
				registry.read(memory_reader(entry.get(file)));
			} else {
				folder->Append(ISO_ptr<FPakFileFile>(get(j.a), this, entry));
			}
		}
	}

	auto	assetroot = ISO::Browser2(root)[0]["Content"];
	for (auto &i : registry.AssetDataBuffer) {
		if (auto file = assetroot.Parse((const char*)i.PackageName).check<FPakFileFile>())
			file->asset = &i;
	}
}

class UnrealPAK : public FileHandler {
	const char* GetExt() override { return "pak"; }
	const char* GetDescription() override { return "Unreal pak"; }

	ISO_ptr<void> Read(tag id, istream_ref file) override {
		FPakFile	pak;
		if (pak.read(file)) {
			return ISO_ptr<FPakFileRoot>(id, file, pak);
		}
		return ISO_NULL;
	}
} unreal_pak;


class UnrealAssetReg : public FileHandler {
	const char* GetDescription() override { return "Unreal Asset Registry"; }

	ISO_ptr<void> Read(tag id, istream_ref file) override {
		ISO_ptr<FAssetRegistryState>	p(id);
		p->read(file);
		return p;
	}
} unreal_assetreg;