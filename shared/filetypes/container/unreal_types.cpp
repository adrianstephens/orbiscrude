#include "iso/iso_files.h"
#include "hashes/fnv.h"
#include "comms/zlib_stream.h"
#include "utilities.h"
#include "disassembler.h"
#include "3d/model_utils.h"
#include "archive_help.h"

using namespace iso;

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

	VER_UE4_AUTOMATIC_VERSION_PLUS_ONE,
	VER_UE4_AUTOMATIC_VERSION = VER_UE4_AUTOMATIC_VERSION_PLUS_ONE - 1
};

enum EObjectFlags {
	RF_NoFlags					= 0x00000000,	///< No flags, used to avoid a cast

	// This first group of flags mostly has to do with what kind of object it is. Other than transient, these are the persistent object flags.
	// The garbage collector also tends to look at these.
	RF_Public					=0x00000001,	///< Object is visible outside its package.
	RF_Standalone				=0x00000002,	///< Keep object around for editing even if unreferenced.
	RF_MarkAsNative				=0x00000004,	///< Object (UField) will be marked as native on construction (DO NOT USE THIS FLAG in HasAnyFlags() etc)
	RF_Transactional			=0x00000008,	///< Object is transactional.
	RF_ClassDefaultObject		=0x00000010,	///< This object is its class's default object
	RF_ArchetypeObject			=0x00000020,	///< This object is a template for another object - treat like a class default object
	RF_Transient				=0x00000040,	///< Don't save object.

	// This group of flags is primarily concerned with garbage collection.
	RF_MarkAsRootSet			=0x00000080,	///< Object will be marked as root set on construction and not be garbage collected, even if unreferenced (DO NOT USE THIS FLAG in HasAnyFlags() etc)
	RF_TagGarbageTemp			=0x00000100,	///< This is a temp user flag for various utilities that need to use the garbage collector. The garbage collector itself does not interpret it.

	// The group of flags tracks the stages of the lifetime of a uobject
	RF_NeedInitialization		=0x00000200,	///< This object has not completed its initialization process. Cleared when ~FObjectInitializer completes
	RF_NeedLoad					=0x00000400,	///< During load, indicates object needs loading.
	RF_KeepForCooker			=0x00000800,	///< Keep this object during garbage collection because it's still being used by the cooker
	RF_NeedPostLoad				=0x00001000,	///< Object needs to be postloaded.
	RF_NeedPostLoadSubobjects	=0x00002000,	///< During load, indicates that the object still needs to instance subobjects and fixup serialized component references
	RF_NewerVersionExists		=0x00004000,	///< Object has been consigned to oblivion due to its owner package being reloaded, and a newer version currently exists
	RF_BeginDestroyed			=0x00008000,	///< BeginDestroy has been called on the object.
	RF_FinishDestroyed			=0x00010000,	///< FinishDestroy has been called on the object.

	// Misc. Flags
	RF_BeingRegenerated			=0x00020000,	///< Flagged on UObjects that are used to create UClasses (e.g. Blueprints) while they are regenerating their UClass on load (See FLinkerLoad::CreateExport())
	RF_DefaultSubObject			=0x00040000,	///< Flagged on subobjects that are defaults
	RF_WasLoaded				=0x00080000,	///< Flagged on UObjects that were loaded
	RF_TextExportTransient		=0x00100000,	///< Do not export object to text form (e.g. copy/paste). Generally used for sub-objects that can be regenerated from data in their parent object.
	RF_LoadCompleted			=0x00200000,	///< Object has been completely serialized by linkerload at least once. DO NOT USE THIS FLAG, It should be replaced with RF_WasLoaded.
	RF_InheritableComponentTemplate = 0x00400000, ///< Archetype of the object can be in its super class
	RF_DuplicateTransient		=0x00800000,	///< Object should not be included in any type of duplication (copy/paste, binary duplication, etc.)
	RF_StrongRefOnFrame			=0x01000000,	///< References to this object from persistent function frame are handled as strong ones.
	RF_NonPIEDuplicateTransient	=0x02000000,	///< Object should not be included for duplication unless it's being duplicated for a PIE session
	RF_Dynamic					=0x04000000,	///< Field Only. Dynamic field - doesn't get constructed during static initialization, can be constructed multiple times
	RF_WillBeLoaded				=0x08000000,	///< This object was constructed during load and will be loaded shortly

	RF_AllFlags					= 0x0fffffff,
	RF_Load						= RF_Public | RF_Standalone | RF_Transactional | RF_ClassDefaultObject | RF_ArchetypeObject | RF_DefaultSubObject | RF_TextExportTransient | RF_InheritableComponentTemplate | RF_DuplicateTransient | RF_NonPIEDuplicateTransient,
	RF_PropagateToSubObjects	= RF_Public | RF_ArchetypeObject | RF_Transactional | RF_Transient,
};

enum EPropertyFlags : uint64 {
	CPF_None = 0,

	CPF_Edit							= 0x0000000000000001,	///< Property is user-settable in the editor.
	CPF_ConstParm						= 0x0000000000000002,	///< This is a constant function parameter
	CPF_BlueprintVisible				= 0x0000000000000004,	///< This property can be read by blueprint code
	CPF_ExportObject					= 0x0000000000000008,	///< Object can be exported with actor.
	CPF_BlueprintReadOnly				= 0x0000000000000010,	///< This property cannot be modified by blueprint code
	CPF_Net								= 0x0000000000000020,	///< Property is relevant to network replication.
	CPF_EditFixedSize					= 0x0000000000000040,	///< Indicates that elements of an array can be modified, but its size cannot be changed.
	CPF_Parm							= 0x0000000000000080,	///< Function/When call parameter.
	CPF_OutParm							= 0x0000000000000100,	///< Value is copied out after function call.
	CPF_ZeroConstructor					= 0x0000000000000200,	///< memset is fine for construction
	CPF_ReturnParm						= 0x0000000000000400,	///< Return value.
	CPF_DisableEditOnTemplate			= 0x0000000000000800,	///< Disable editing of this property on an archetype/sub-blueprint
	//CPF_      						= 0x0000000000001000,	///< 
	CPF_Transient   					= 0x0000000000002000,	///< Property is transient: shouldn't be saved or loaded, except for Blueprint CDOs.
	CPF_Config      					= 0x0000000000004000,	///< Property should be loaded/saved as permanent profile.
	//CPF_								= 0x0000000000008000,	///< 
	CPF_DisableEditOnInstance			= 0x0000000000010000,	///< Disable editing on an instance of this class
	CPF_EditConst   					= 0x0000000000020000,	///< Property is uneditable in the editor.
	CPF_GlobalConfig					= 0x0000000000040000,	///< Load config from base class, not subclass.
	CPF_InstancedReference				= 0x0000000000080000,	///< Property is a component references.
	//CPF_								= 0x0000000000100000,	///<
	CPF_DuplicateTransient				= 0x0000000000200000,	///< Property should always be reset to the default value during any type of duplication (copy/paste, binary duplication, etc.)
	CPF_SubobjectReference				= 0x0000000000400000,	///< Property contains subobject references (TSubobjectPtr)
	//CPF_    							= 0x0000000000800000,	///< 
	CPF_SaveGame						= 0x0000000001000000,	///< Property should be serialized for save games, this is only checked for game-specific archives with ArIsSaveGame
	CPF_NoClear							= 0x0000000002000000,	///< Hide clear (and browse) button.
	//CPF_  							= 0x0000000004000000,	///<
	CPF_ReferenceParm					= 0x0000000008000000,	///< Value is passed by reference; CPF_OutParam and CPF_Param should also be set.
	CPF_BlueprintAssignable				= 0x0000000010000000,	///< MC Delegates only.  Property should be exposed for assigning in blueprint code
	CPF_Deprecated  					= 0x0000000020000000,	///< Property is deprecated.  Read it from an archive, but don't save it.
	CPF_IsPlainOldData					= 0x0000000040000000,	///< If this is set, then the property can be memcopied instead of CopyCompleteValue / CopySingleValue
	CPF_RepSkip							= 0x0000000080000000,	///< Not replicated. For non replicated properties in replicated structs 
	CPF_RepNotify						= 0x0000000100000000,	///< Notify actors when a property is replicated
	CPF_Interp							= 0x0000000200000000,	///< interpolatable property for use with matinee
	CPF_NonTransactional				= 0x0000000400000000,	///< Property isn't transacted
	CPF_EditorOnly						= 0x0000000800000000,	///< Property should only be loaded in the editor
	CPF_NoDestructor					= 0x0000001000000000,	///< No destructor
	//CPF_								= 0x0000002000000000,	///<
	CPF_AutoWeak						= 0x0000004000000000,	///< Only used for weak pointers, means the export type is autoweak
	CPF_ContainsInstancedReference		= 0x0000008000000000,	///< Property contains component references.
	CPF_AssetRegistrySearchable			= 0x0000010000000000,	///< asset instances will add properties with this flag to the asset registry automatically
	CPF_SimpleDisplay					= 0x0000020000000000,	///< The property is visible by default in the editor details view
	CPF_AdvancedDisplay					= 0x0000040000000000,	///< The property is advanced and not visible by default in the editor details view
	CPF_Protected						= 0x0000080000000000,	///< property is protected from the perspective of script
	CPF_BlueprintCallable				= 0x0000100000000000,	///< MC Delegates only.  Property should be exposed for calling in blueprint code
	CPF_BlueprintAuthorityOnly			= 0x0000200000000000,	///< MC Delegates only.  This delegate accepts (only in blueprint) only events with BlueprintAuthorityOnly.
	CPF_TextExportTransient				= 0x0000400000000000,	///< Property shouldn't be exported to text format (e.g. copy/paste)
	CPF_NonPIEDuplicateTransient		= 0x0000800000000000,	///< Property should only be copied in PIE
	CPF_ExposeOnSpawn					= 0x0001000000000000,	///< Property is exposed on spawn
	CPF_PersistentInstance				= 0x0002000000000000,	///< A object referenced by the property is duplicated like a component. (Each actor should have an own instance.)
	CPF_UObjectWrapper					= 0x0004000000000000,	///< Property was parsed as a wrapper class like TSubclassOf<T>, FScriptInterface etc., rather than a USomething*
	CPF_HasGetValueTypeHash				= 0x0008000000000000,	///< This property can generate a meaningful hash value.
	CPF_NativeAccessSpecifierPublic		= 0x0010000000000000,	///< Public native access specifier
	CPF_NativeAccessSpecifierProtected	= 0x0020000000000000,	///< Protected native access specifier
	CPF_NativeAccessSpecifierPrivate	= 0x0040000000000000,	///< Private native access specifier
	CPF_SkipSerialization				= 0x0080000000000000,	///< Property shouldn't be serialized, can still be exported to text

	CPF_NativeAccessSpecifiers			= CPF_NativeAccessSpecifierPublic | CPF_NativeAccessSpecifierProtected | CPF_NativeAccessSpecifierPrivate,	/** All Native Access Specifier flags */
	CPF_ParmFlags						= CPF_Parm | CPF_OutParm | CPF_ReturnParm | CPF_ReferenceParm | CPF_ConstParm,	/** All parameter flags */

														/** Flags that are propagated to properties inside containers */
	CPF_PropagateToArrayInner			= CPF_ExportObject | CPF_PersistentInstance | CPF_InstancedReference | CPF_ContainsInstancedReference | CPF_Config | CPF_EditConst | CPF_Deprecated | CPF_EditorOnly | CPF_AutoWeak | CPF_UObjectWrapper,
	CPF_PropagateToMapValue				= CPF_ExportObject | CPF_PersistentInstance | CPF_InstancedReference | CPF_ContainsInstancedReference | CPF_Config | CPF_EditConst | CPF_Deprecated | CPF_EditorOnly | CPF_AutoWeak | CPF_UObjectWrapper | CPF_Edit,
	CPF_PropagateToMapKey				= CPF_ExportObject | CPF_PersistentInstance | CPF_InstancedReference | CPF_ContainsInstancedReference | CPF_Config | CPF_EditConst | CPF_Deprecated | CPF_EditorOnly | CPF_AutoWeak | CPF_UObjectWrapper | CPF_Edit,
	CPF_PropagateToSetElement			= CPF_ExportObject | CPF_PersistentInstance | CPF_InstancedReference | CPF_ContainsInstancedReference | CPF_Config | CPF_EditConst | CPF_Deprecated | CPF_EditorOnly | CPF_AutoWeak | CPF_UObjectWrapper | CPF_Edit,
	
	CPF_InterfaceClearMask				= CPF_ExportObject|CPF_InstancedReference|CPF_ContainsInstancedReference,	/** The flags that should never be set on interface properties */
	CPF_DevelopmentAssets				= CPF_EditorOnly,	/** All the properties that can be stripped for final release console builds */
	CPF_ComputedFlags					= CPF_IsPlainOldData | CPF_NoDestructor | CPF_ZeroConstructor | CPF_HasGetValueTypeHash,	/** All the properties that should never be loaded or saved */
	CPF_AllFlags						= 0xFFFFFFFFFFFFFFFF,	/** Mask of all property flags */
};

enum EClassFlags {
	CLASS_None						= 0x00000000u,
	CLASS_Abstract					= 0x00000001u,
	CLASS_DefaultConfig				= 0x00000002u,
	CLASS_Config					= 0x00000004u,
	CLASS_Transient					= 0x00000008u,
	CLASS_Parsed					= 0x00000010u,
	CLASS_MatchedSerializers		= 0x00000020u,
	CLASS_AdvancedDisplay			= 0x00000040u,
	CLASS_Native					= 0x00000080u,
	CLASS_NoExport					= 0x00000100u,
	CLASS_NotPlaceable				= 0x00000200u,
	CLASS_PerObjectConfig			= 0x00000400u,
	CLASS_ReplicationDataIsSetUp	= 0x00000800u,
	CLASS_EditInlineNew				= 0x00001000u,
	CLASS_CollapseCategories		= 0x00002000u,
	CLASS_Interface					= 0x00004000u,
	CLASS_CustomConstructor			= 0x00008000u,
	CLASS_Const						= 0x00010000u,
	CLASS_LayoutChanging			= 0x00020000u,
	CLASS_CompiledFromBlueprint		= 0x00040000u,
	CLASS_MinimalAPI				= 0x00080000u,
	CLASS_RequiredAPI				= 0x00100000u,
	CLASS_DefaultToInstanced		= 0x00200000u,
	CLASS_TokenStreamAssembled		= 0x00400000u,
	CLASS_HasInstancedReference		= 0x00800000u,
	CLASS_Hidden					= 0x01000000u,
	CLASS_Deprecated				= 0x02000000u,
	CLASS_HideDropDown				= 0x04000000u,
	CLASS_GlobalUserConfig			= 0x08000000u,
	CLASS_Intrinsic					= 0x10000000u,
	CLASS_Constructed				= 0x20000000u,
	CLASS_ConfigDoNotCheckDefaults	= 0x40000000u,
	CLASS_NewerVersionExists		= 0x80000000u,
};

enum ELifetimeCondition : uint8 {
	COND_None						= 0,
	COND_InitialOnly				= 1,
	COND_OwnerOnly					= 2,
	COND_SkipOwner					= 3,
	COND_SimulatedOnly				= 4,
	COND_AutonomousOnly				= 5,
	COND_SimulatedOrPhysics			= 6,
	COND_InitialOrOwner				= 7,
	COND_Custom						= 8,
	COND_ReplayOrOwner				= 9,
	COND_ReplayOnly					= 10,	
	COND_SimulatedOnlyNoReplay		= 11,
	COND_SimulatedOrPhysicsNoReplay	= 12,
	COND_SkipReplay					= 13,	
	COND_Never						= 15,			
	COND_Max						= 16,			
};

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

template<typename T> class TArray : public dynamic_array<T> {
public:
	using dynamic_array<T>::dynamic_array;
	using dynamic_array<T>::read;
	template<typename R> bool read(R& file) {
		return dynamic_array<T>::read(file, file.template get<int>());
	}
//	bool read(struct istream_linker &file) {
//		return dynamic_array<T>::read(file, file.get<int>());
//	}
};

template<typename T> struct TBulkArray : TArray<T> {
	template<typename R> enable_if_t<has_read<R, T>, bool> read(R file) {
		return TArray<T>::read(file);
	}
	template<typename R> enable_if_t<!has_read<R, T>, bool> read(R file) {
		auto	size = file.template get<uint32>();
		ISO_ASSERT(size == sizeof(T));
		return TArray<T>::read(file);
	}
};

struct TBitArray : dynamic_bitarray<uint32> {
	bool read(istream_ref file) {
		resize(file.get<uint32>());
		return check_readbuff(file, data(), num_t() * sizeof(uint32));
	}
};

template<typename T> struct TSparseArray : sparse_array<T, uint32, uint32> {
	typedef	sparse_array<T, uint32, uint32>	B;
	T&	operator[](int i) const	{ return B::get(i); }
	bool read(istream_ref file) {
		TBitArray AllocatedIndices;
		file.read(AllocatedIndices);

		resize(AllocatedIndices.size());
		for (auto i : AllocatedIndices.all<true>())
			file.read(B::put(i));
		return true;
	}
};

template<typename K, typename V> struct TMap : public map<K, V> {
	template<typename R> bool read(R&& file) {
		int		len = file.template get<int>();
		while (len-- && !file.eof()) {
			K	k = file.template get<K>();
			V	v = file.template get<V>();
			put(move(k), move(v));
		}
		return true;
	}
};

template<typename K> struct TSet : public set<K> {
	template<typename R> bool read(R&& file) {
		int		len = file.template get<int>();
		while (len-- && !file.eof())
			insert(file.template get<K>());
		return true;
	}
};
struct bool32 {
	bool	v;
	operator bool() const { return v; }
	bool read(istream_ref file) {
		auto	x = file.get<uint32>();
		v = !!x;
		return x < 2;
	}
};

struct FVector2D	{ float	X, Y; };
struct FVector		{ float	X, Y, Z; };
struct FVector4		{ float	X, Y, Z, W; };
struct FQuat		{ float	X, Y, Z, W; };
struct FIntPoint	{ int	X, Y; };
struct FIntVector	{ int	X, Y, Z; };
struct FColor		{ uint8	B, G, R, A; };
struct FLinearColor	{ float	R, G, B, A; };
struct FRotator		{ float Pitch, Yaw, Roll; };
struct FMatrix		{ FVector4	X, Y, Z, W; };

typedef FVector4	FPlane;

// note: no Raw serialiser
struct FTransform {
	FQuat	Rotation;
	FVector	Translation;
	FVector	Scale3D;
};

struct FBox {
	FVector Min;
	FVector Max;
	uint8 IsValid;
	bool read(istream_ref file) { return iso::read(file, Min, Max, IsValid); }
};

struct FSphere {
	FVector Center;
	float	W;
};

// note: no Raw serialiser
struct FBoxSphereBounds {
	FVector	Origin;
	FVector BoxExtent;
	float	SphereRadius;
};

struct FGuid {
	xint32 A, B, C, D;
	FGuid() : A(0), B(0), C(0), D(0) {}
	FGuid(uint32 A, uint32 B, uint32 C, uint32 D) : A(A), B(B), C(C), D(D) {}
};

class FString : comparisons<FString> {
	char *p;

	static char* make(const char *s, const char *e)	{ size_t len = e - s; char *p = (char*)iso::malloc(len + 2) + 1; memcpy(p, s, len); p[len] = 0; return p; }
	static char* make(const char *s)				{ size_t len = string_len(s) + 1; char *p = (char*)iso::malloc(len + 1) + 1; memcpy(p, s, len); return p; }
	static char* make(const char16 *s)				{ size_t len = string_len(s) + 1; char *p = (char*)iso::malloc(len * 2); memcpy(p, s, len * 2); return p; }

	void	clear()					{ if (p) iso::free(align_down(p, 2)); p = nullptr; }
public:
	FString()						: p(0) {}
	FString(FString &&s)			: p(exchange(s.p, nullptr))	{}
	FString(const char *s)			: p(s ? make(s) : nullptr) {}
	FString(const char *s, const char *e)	: p(make(s, e)) {}
	FString(const count_string &s)	: p(make(s.begin(), s.end())) {}
	FString(const char16 *s)		: p(s ? make(s) : nullptr) {}
	FString(const FString &s)		: p(!s ? nullptr : s.is_wide() ? make((char16*)s.p) : make((char*)s.p)) {}
	~FString()						{ clear(); }

	FString& operator=(FString &&s)	{ swap(p, s.p); return *this; }
	explicit operator bool() const	{ return !!p; }
	explicit operator const char*() const	{ return is_wide() ? 0 : (const char*)p; }
//	operator tag2()	const			{ return operator const char*(); }

	friend const char*	get(const FString &s)	{ return (const char*)s; }

	bool	is_wide()	const		{ return p && !(intptr_t(p) & 1); }
	size_t	length()	const		{ return is_wide() ? string_len((char16*)p) : string_len((char*)p); }

	template<typename T>	friend int	compare(const FString &s1, const T &s2) {
		return	!s1	? (!s2 ? 0 : -1)
			:	!s2	? 1
			:	s1.is_wide() ? -s2.compare_to((char16*)s1.p)
			:	-s2.compare_to((char*)s1.p);
	}

	template<typename C> int	compare_to(const C *s) const {
		return is_wide()
			? -str(s).compare_to((char16*)p)
			: -str(s).compare_to((char*)p);
	}

	bool read(istream_ref file) {
		clear();
		int len;
		if (!file.read(len))
			return false;
		if (len < 0) {
			p = (char*)iso::malloc(-len * 2);
			return check_readbuff(file, p, -len * 2);
		} else if (len > 0) {
			p = (char*)iso::malloc(len + 1) + 1;
			return check_readbuff(file, p, len);
		}
		return true;
	}
	friend uint64	hash(const FString &s) {
		return s.is_wide()
			? FNV1<64>((char16*)s.p, string_len((char16*)s.p))
			: FNV1<64>((char*)s.p);
	}
	friend tag2	_GetName(const FString &s)		{ return (const char*)s; }

};

typedef FString FText;

struct FNameEntryHeader {
	uint16 bIsWide : 1;
#if WITH_CASE_PRESERVING_NAME
	uint16 Len : 15;
#else
	uint16 LowercaseProbeHash:5, Len:10;
#endif
};

struct FNameEntry : FNameEntryHeader {
	enum { NAME_SIZE = 1024 };
	union {
		char   AnsiName[NAME_SIZE];
		char16 WideName[NAME_SIZE];
	};
};

struct FNameEntrySerialized : FNameEntry {
	bool read(istream_ref file, EUnrealEngineObjectUE4Version ver) {
		int32 len0 = file.get();
		if (bIsWide = len0 < 0)
			readn(file, WideName, Len = -len0);
		else
			readn(file, AnsiName, Len = len0);

		if (ver >= VER_UE4_NAME_HASHES_SERIALIZED) {
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

struct FName {
	uint32	ComparisonIndex;
	uint32	Number;
	FName(uint32 ComparisonIndex = ~0, uint32 Number = ~0) : ComparisonIndex(ComparisonIndex), Number(Number) {}
	explicit constexpr operator bool() const { return ~ComparisonIndex; }
	bool operator==(const FName &b)	const { return ComparisonIndex == b.ComparisonIndex && Number == b.Number; }
	bool operator!=(const FName &b)	const { return !(*this == b); }
	bool operator<(const FName &b)	const { return ComparisonIndex < b.ComparisonIndex ||  (ComparisonIndex == b.ComparisonIndex && Number == b.Number); }
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
		return iso::read(file, Protocol, Host, Map, Portal, Op, Port, Valid);
	}
};

struct FCustomVersionContainer {
	enum Type {
		Unknown,
		Guids,
		Enums,
		Optimized,
		// Add new versions above this comment
		CustomVersion_Automatic_Plus_One,
		Latest = CustomVersion_Automatic_Plus_One - 1
	};

	struct FCustomVersion {
		FGuid	Key;
		int32	Version;
		FString FriendlyName;
		FCustomVersion() {}
		FCustomVersion(FGuid InKey, int32 InVersion, FString&& FriendlyName) : Key(InKey), Version(InVersion), FriendlyName(move(FriendlyName)) {}
		bool read(istream_ref file)		{ return iso::read(file, Key, Version); }
	};

	struct FEnumCustomVersion_DEPRECATED {
		uint32	Tag;
		int32	Version;
		operator FCustomVersion() const	{ return FCustomVersion(FGuid(0, 0, 0, Tag), Version, format_string("EnumTag%u", Tag).begin()); }
		bool read(istream_ref file)		{ return iso::read(file, Tag, Version); }
	};

	struct FGuidCustomVersion_DEPRECATED {
		FGuid	Key;
		int32	Version;
		FString FriendlyName;
		operator FCustomVersion() const	{ return FCustomVersion(Key, Version, FString(FriendlyName)); }
		bool read(istream_ref file)		{ return iso::read(file, Key, Version, FriendlyName); }
	};

	TArray<FCustomVersion> Versions;

	bool read(istream_ref file, int32 LegacyFileVersion) {
		if (LegacyFileVersion < -5)
			Versions.read(file);
		else if (LegacyFileVersion < -2)
			Versions = TArray<FGuidCustomVersion_DEPRECATED>::static_read(file, file.get<int>());
		else if (LegacyFileVersion == -2)
			Versions = TArray<FEnumCustomVersion_DEPRECATED>::static_read(file, file.get<int>());
		else
			return false;
		return true;
	}

	bool read(istream_ref file, Type Format) {
		switch (Format) {
			case Enums:
				Versions = TArray<FEnumCustomVersion_DEPRECATED>::static_read(file, file.get<int>());
				return true;
			case Guids:
				Versions = TArray<FGuidCustomVersion_DEPRECATED>::static_read(file, file.get<int>());
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
		return iso::read(file, Major, Minor,Patch, Changelist, Branch);
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
		PKG_NewlyCreated				= 0x00000001,	// Newly created package, not saved yet. In editor only.
		PKG_ClientOptional				= 0x00000002,	// Purely optional for clients.
		PKG_ServerSideOnly				= 0x00000004,   // Only needed on the server side.
		PKG_CompiledIn					= 0x00000010,   // This package is from "compiled in" classes.
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
		PKG_ReloadingForCooker			= 0x40000000,   // This package is reloading in the cooker, try to avoid getting data we will never need. We won't save this package.
		PKG_FilterEditorOnly			= 0x80000000,	// Package has editor-only data filtered out
	};

	struct FGenerationInfo {
		int32	ExportCount;
		int32	NameCount;
		bool  read(istream_ref file) { return iso::read(file, ExportCount, NameCount); }
	};

	int32	Tag;

	int32	FileVersionUE4;
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
	FGuid	OwnerPersistentGuid;				// Package persistent owner for this package

	TArray<FGenerationInfo> Generations;		// Data about previous versions of this package

	FEngineVersion SavedByEngineVersion;		// Engine version this package was saved with
	FEngineVersion CompatibleWithEngineVersion;	// Engine version this package is compatible with. See SavedByEngineVersion.

	uint32	CompressionFlags;					// Flags used to compress the file on save and uncompress on load.
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

	// only keep loading if we match the magic
	if (Tag != PACKAGE_FILE_TAG && Tag != swap_endian(PACKAGE_FILE_TAG))
		return false;

	// The package has been stored in a separate endianness than the linker expected so we need to force endian conversion
	// Latent handling allows the PC version to retrieve information about cooked packages
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
		*/
	const int32 CurrentLegacyFileVersion = -7;
	int32		LegacyFileVersion;

	if (!file.read(LegacyFileVersion) || LegacyFileVersion >= 0 || LegacyFileVersion < CurrentLegacyFileVersion)
		return false;	// This is probably an old UE3 file, make sure that the linker will fail to load with it.

	if (LegacyFileVersion != -4) {
		int32 LegacyUE3Version = 0;
		file.read(LegacyUE3Version);
	}

	file.read(FileVersionUE4);
	file.read(FileVersionLicenseeUE4);

	if (LegacyFileVersion <= -2)
		CustomVersionContainer.read(file, LegacyFileVersion);

	if (!FileVersionUE4 && !FileVersionLicenseeUE4)
		return false;

	iso::read(file, TotalHeaderSize, FolderName, PackageFlags, NameCount, NameOffset);

	if (FileVersionUE4 >= VER_UE4_ADDED_PACKAGE_SUMMARY_LOCALIZATION_ID)
		file.read(LocalizationId);

	if (FileVersionUE4 >= VER_UE4_SERIALIZE_TEXT_IN_PACKAGES)
		iso::read(file, GatherableTextDataCount, GatherableTextDataOffset);

	iso::read(file, ExportCount, ExportOffset, ImportCount, ImportOffset, DependsOffset);

	if (FileVersionUE4 >= VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP)
		iso::read(file, SoftPackageReferencesCount, SoftPackageReferencesOffset);

	if (FileVersionUE4 >= VER_UE4_ADDED_SEARCHABLE_NAMES)
		file.read(SearchableNamesOffset);

	iso::read(file, ThumbnailTableOffset, Guid);

	if (FileVersionUE4 >= VER_UE4_ADDED_PACKAGE_OWNER)
		iso::read(file, PersistentGuid, OwnerPersistentGuid);

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
		// handle conversion of single ChunkID to an array of ChunkIDs
		int ChunkID;
		if (file.read(ChunkID) && ChunkID >= 0)
			ChunkIDs.push_back(ChunkID);
	}

	if (FileVersionUE4 >= VER_UE4_PRELOAD_DEPENDENCIES_IN_COOKED_EXPORTS)
		iso::read(file, PreloadDependencyCount, PreloadDependencyOffset);

	return true;
}

struct FBulkData {
	enum EBulkDataFlags {
		None							= 0,			
		PayloadAtEndOfFile				= 1 << 0,		// If set, payload is stored at the end of the file and not inline
		SerializeCompressedZLIB			= 1 << 1,		// If set, payload should be [un]compressed using ZLIB during serialization.
		ForceSingleElementSerialization	= 1 << 2,		// Force usage of SerializeElement over bulk serialization.
		SingleUse						= 1 << 3,		// Bulk data is only used once at runtime in the game.
		Unused							= 1 << 5,		// Bulk data won't be used and doesn't need to be loaded
		ForceInlinePayload				= 1 << 6,		// Forces the payload to be saved inline, regardless of its size
		ForceStreamPayload				= 1 << 7,		// Forces the payload to be always streamed, regardless of its size
		PayloadInSeperateFile			= 1 << 8,		// If set, payload is stored in a .upack file alongside the uasset
		SerializeCompressedBitWindow	= 1 << 9,		// DEPRECATED: If set, payload is compressed using platform specific bit window
		Force_NOT_InlinePayload			= 1 << 10,		// There is a new default to inline unless you opt out
		OptionalPayload					= 1 << 11,		// This payload is optional and may not be on device
		MemoryMappedPayload				= 1 << 12,		// This payload will be memory mapped, this requires alignment, no compression etc.
		Size64Bit						= 1 << 13,		// Bulk data size is 64 bits long
		DuplicateNonOptionalPayload		= 1 << 14,		// Duplicate non-optional payload in optional bulk data.
		BadDataVersion					= 1 << 15,		// Indicates that an old ID is present in the data, at some point when the DDCs are flushed we can remove this.
	};
	uint32	flags;
	uint64	element_count;
	uint64	disk_size;
	uint64	file_offset;

	bool	read(istream_ref file) {
		file.read(flags);
		if (flags & Size64Bit) {
			file.read(element_count);
			file.read(disk_size);
		} else {
			element_count	= file.get<uint32>();
			disk_size		= file.get<uint32>();
		}
		return file.read(file_offset);
	}

	istream_ptr		reader(istream_ref file, streamptr bulk_offset) const {
		streamptr	offset = flags & PayloadAtEndOfFile ? bulk_offset + file_offset : file_offset;
		file.seek(offset);

		if (flags & SerializeCompressedZLIB) {
			// Serialize package file tag used to determine endianess.
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

			return new reader_mixout<memory_reader_owner>(move(mem));
		}

		return new istream_offset(file, disk_size);
	}
};

struct FPackageIndex {
	int32			Index;

	FPackageIndex() : Index(0) {}
	FPackageIndex(int32 Index, bool import) : Index(import ? -Index - 1 : Index + 1) {}
	bool operator<(FPackageIndex b) const { return Index < b.Index; }

	bool	IsImport()	const	{ return Index < 0; }
	bool	IsExport()	const	{ return Index > 0; }
	bool	IsNull()	const	{ return Index == 0; }
	int32	ToImport()	const	{ ISO_ASSERT(IsImport()); return -Index - 1; }
	int32	ToExport()	const	{ ISO_ASSERT(IsExport()); return Index - 1; }
};

struct FObjectResource {
	FName			ObjectName;
	FPackageIndex	OuterIndex;
	ISO_ptr<ISO_ptr<void>>	p;
};

// UObject resource type for objects that are referenced by this package, but contained within another package
struct FObjectImport : public FObjectResource {
	FName			ClassPackage;
	FName			ClassName;
	bool read(istream_ref file) {
		return iso::read(file, ClassPackage, ClassName, OuterIndex, ObjectName);
	}
};

// UObject resource type for objects that are contained within this package and can be referenced by other packages.
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

	FGuid			PackageGuid;
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

		iso::read(file, bForcedExport, bNotForClient, bNotForServer, PackageGuid, PackageFlags);

		if (ver >= VER_UE4_LOAD_FOR_EDITOR_GAME)
			file.read(bNotAlwaysLoadedForEditorGame);

		if (ver >= VER_UE4_COOKED_ASSETS_IN_EDITOR_SUPPORT)
			file.read(bIsAsset);

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
	bool							from_summary		= false;
	int64							BulkDataStartOffset = 0;
//	const FPackageFileSummary		*sum;
	TArray<FString>					NameMap;
	TArray<FObjectImport>			ImportMap;
	TArray<FObjectExport>			ExportMap;
	TArray<TArray<FPackageIndex> >	DependsMap;
	TArray<FName>					SoftPackageReferenceList;
	TMap<FPackageIndex, TArray<FName>> SearchableNamesMap;

	FLinkerTables() {
	}
	
	FLinkerTables(istream_ref file, const FPackageFileSummary& sum) : from_summary(true), BulkDataStartOffset(sum.BulkDataStartOffset) {

		if (sum.NameCount > 0) {
			file.seek(sum.NameOffset);
			NameMap.resize(sum.NameCount);
			for (auto &i : NameMap) {
				FNameEntrySerialized	name;
				name.read(file, (EUnrealEngineObjectUE4Version)sum.FileVersionUE4);
				i = name;
			}
		}

		if (sum.ImportCount > 0) {
			file.seek(sum.ImportOffset);
			ImportMap.read(file, sum.ImportCount);
		}

		if (sum.ExportCount > 0) {
			file.seek(sum.ExportOffset);
			ExportMap.resize(sum.ExportCount);
			for (auto &i : ExportMap)
				i.read(file, (EUnrealEngineObjectUE4Version)sum.FileVersionUE4);
		}


		if (sum.FileVersionUE4 >= VER_UE4_ADD_STRING_ASSET_REFERENCES_MAP && sum.SoftPackageReferencesOffset > 0 && sum.SoftPackageReferencesCount > 0) {
			file.seek(sum.SoftPackageReferencesOffset);

			if (sum.FileVersionUE4 < VER_UE4_ADDED_SOFT_OBJECT_PATH) {
				for (int32 ReferenceIdx = 0; ReferenceIdx < sum.SoftPackageReferencesCount; ++ReferenceIdx) {
					FString PackageName;
					file.read(PackageName);
					// OutDependencyData.SoftPackageReferenceList.push_back(PackageName);
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
};

struct istream_linker : reader<istream_linker>, reader_ref<istream_ref> {
	FLinkerTables	*linker;
	istream_linker(istream_ref file, FLinkerTables *linker) : reader_ref<istream_ref>(file), linker(linker) {}

	auto	bulk_data() {
		return linker->BulkDataStartOffset;
	}

	cstring	lookup(const FName& name) const {
		return linker ? linker->lookup(name) : "None";
	}

	const FObjectResource	*lookup(FPackageIndex index) const {
		return linker ? linker->lookup(index) : nullptr;
	}

	friend bool	read(istream_linker &file, FName& name) {
		if (file.linker->from_summary)
			return iso::read((reader_ref<istream_ref>&)file, name);

		name = file.linker->add_name(file.get<FString>());
		return true;
	}
	friend bool	read(istream_linker &file, FPackageIndex& pkg) {
		if (file.linker->from_summary)
			return iso::read((reader_ref<istream_ref>&)file, pkg);

		FObjectImport	obj;
		file.read(obj.ObjectName);
		pkg = file.linker->add_package(move(obj));
		return true;
	}
};


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
	FName	Name;					// Name of property.
	FName	StructName;				// Struct name if FStructProperty.
	FName	EnumName;				// Enum name if FByteProperty or FEnumProperty
	FName	InnerType;				// Inner type if FArrayProperty, FSetProperty, or FMapProperty
	FName	ValueType;				// Value type if UMapPropery
	int32	Size			= 0;	// Property size.
	int32	ArrayIndex		= -1;	// Index if an array; else 0.
	int64	SizeOffset		= -1;	// location in stream of tag size member
	FGuid	StructGuid;
	uint8	HasPropertyGuid	= 0;
	FGuid	PropertyGuid;

	bool read(istream_linker& file) {
		*this = FPropertyTag();

		if (!file.read(Name) || file.lookup(Name) == "None")
			return false;

		if (!iso::read(file, Type, Size, ArrayIndex))
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
	ObjectLoader(const char *name,  load_t *load) : static_hash<ObjectLoader, const char*>(name), load(load) {}
};

template<typename T> struct ObjectLoaderRaw : ObjectLoader {
	ObjectLoaderRaw(const char *name) : ObjectLoader(name, [](istream_linker &file)->ISO_ptr<void> {
		return ISO::MakePtr(0, file.get<T>());
	}) {}
};

struct FName2 : FString {
	bool read(istream_linker& file) {
		*(FString*)this = FString(file.lookup(file.get<FName>()));
		return true;
	}
	friend tag2	_GetName(const FName2 &s)		{ return (const char*)s; }
};

struct FSoftObjectPath {
	FName2	AssetPathName;
	FString	SubPathString;
	bool	read(istream_linker& file) {
		return iso::read(file, AssetPathName, SubPathString);
	}
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

uint32	element_size(const char* type) {
	switch (string_hash(type)) {
		case "Int8Property"_fnv:	return sizeof(int8);
		case "Int16Property"_fnv:	return sizeof(int16);
		case "IntProperty"_fnv:		return sizeof(int32);
		case "Int64Property"_fnv:	return sizeof(int64);
		case "UInt16Property"_fnv:	return sizeof(uint16);
		case "UInt32Property"_fnv:	return sizeof(uint32);
		case "UInt64Property"_fnv:	return sizeof(uint64);
		case "FloatProperty"_fnv:	return sizeof(float);
		case "DoubleProperty"_fnv:	return sizeof(double);
		case "ObjectProperty"_fnv:	return sizeof(FPackageIndex);
		case "NameProperty"_fnv:	return sizeof(FName);
		case "BoolProperty"_fnv:	return 1;
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
			ISO_ptr<anything>	array(id);
			auto	size	= file.get<int>();
			auto	inner	= tag.inner();

			if (file.lookup(inner.Type) == "StructProperty")
				file.read(inner);

			for (int i = 0; i < size; i++)
				array->Append(ReadTag(file, inner, 0));
			return array;
		}
		case "SetProperty"_fnv: {
			ISO_ptr<anything>	array(id);
			auto	size	= file.get<int>();
			auto	inner	= tag.inner();

			// Delete any explicitly-removed keys
			if (auto nremove = file.get<int>()) {
				ISO_ptr<anything>	remove("remove");
				array->Append(remove);
				for (int i = 0; i < nremove; i++)
					remove->Append(ReadTag(file, inner, 0));
			}

			for (int i = 0; i < size; i++)
				array->Append(ReadTag(file, inner, 0));
			return array;

		}
		case "MapProperty"_fnv: {
			ISO_ptr<anything>	array(id);
			auto	inner	= tag.inner();
			auto	value	= tag.value();

			// Delete any explicitly-removed keys
			if (auto nremove = file.get<int>()) {
				ISO_ptr<anything>	remove("remove");
				array->Append(remove);
				for (int i = 0; i < nremove; i++)
					remove->Append(ReadTag(file, inner, 0));
			}

			auto	count		= file.get<int>();
			uint32	key_size	= element_size(file.lookup(inner.Type));
			uint32	val_size	= element_size(file.lookup(value.Type));
			bool	known_val	= val_size != 0;
			if (!known_val)
				val_size = (tag.Size - 8) / count - key_size;

			for (int i = 0; i < count; i++) {
				ISO_ptr<pair<ISO_ptr<void>, ISO_ptr<void>>>	p(0);
				p->a = ReadTag(file, inner, 0);
				if (known_val) {
					p->b = ReadTag(file, value, 0);
				} else {
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

			// Delete any explicitly-removed keys
			if (auto nremove = file.get<int>()) {
				for (int i = 0; i < nremove; i++)
					ReadTag(file, inner, 0);
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

			// Delete any explicitly-removed keys
			if (auto nremove = file.get<int>()) {
				for (int i = 0; i < nremove; i++)
					ReadTag(file, inner, 0);
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
struct UObject;
struct UClass;
struct UStruct;
typedef UStruct	UScriptStruct;
struct UEnum;
struct UFunction;
struct FFieldClass;

struct FScriptDelegate;
struct FMulticastScriptDelegate;

struct FScriptArray {
	void	*p;
};

struct FScriptBitArray : FScriptArray {
	int32	NumBits;
	int32	MaxBits;
};

struct FScriptSparseArray {
	struct Layout {
		// ElementOffset is at zero offset from the TSparseArrayElementOrFreeListLink - not stored here
		int32	Alignment;
		int32	Size;
	};
	FScriptArray	Data;
	FScriptBitArray	AllocationFlags;
	int32			FirstFreeIndex;
	int32			NumFreeIndices;
};
struct FScriptSet {
	struct Layout {
		// int32 ElementOffset = 0; // always at zero offset from the TSetElement - not stored here
		int32 HashNextIdOffset;
		int32 HashIndexOffset;
		int32 Size;
		FScriptSparseArray::Layout SparseArrayLayout;
	};
	FScriptSparseArray Elements;
	//typename Allocator::HashAllocator::template ForElementType<FSetElementId> Hash;
	int32    HashSize;
};
struct FScriptMap {
	struct Layout {
		// int32 KeyOffset; // is always at zero offset from the TPair - not stored here
		int32 ValueOffset;
		FScriptSet::Layout SetLayout;
	};

	FScriptSet Pairs;
};

struct _Ptr : ISO_ptr<void> {
	bool read(istream_linker& file) {
		auto	i	= file.get<FPackageIndex>();
		if (auto obj = file.lookup(i))
			*((ISO_ptr<void>*)this) = obj->p;
		return true;
	}
};

template<typename T> struct TPtr : _Ptr {};

struct _InlinePtr : ISO_ptr<void> {
	bool read(istream_linker& file) {
		auto	type	= file.get<FName>();
		if (auto i = ObjectLoader::get(file.lookup(type))) {
			*((ISO_ptr<void>*)this) = i->load(file);
		} else {
			ISO_TRACEF("Needs ") << file.lookup(type) << '\n';
			ISO_ASSERT(0);
		}
		return true;
	}
};

template<typename T> struct TInlinePtr : _InlinePtr {};

template<typename T> struct TKnownPtr : ISO_ptr<T> {
	bool read(istream_linker& file) {
		return file.read(*Create(0));
	}
};

template<typename T> struct TSoftPtr : FSoftObjectPath {};

struct FField {
	FName2					NamePrivate;
	EObjectFlags			FlagsPrivate;
	TMap<FName2, FString>	MetaDataMap;

	bool read(istream_linker& file) {
		if (!file.read(NamePrivate) || !file.read(FlagsPrivate))
			return false;
		if (file.get<bool32>())
			return MetaDataMap.read(file);
		return true;
	}
};

struct FProperty : FField {
	int32			ArrayDim;
	int32			ElementSize;
	EPropertyFlags	PropertyFlags;
	uint16			RepIndex;
	FName2			RepNotifyFunc;
	ELifetimeCondition BlueprintReplicationCondition;

	uint32			GetOffset_ForDebug() const { return 0; }

	bool read(istream_linker& file) {
		return FField::read(file)
			&& iso::read(file, ArrayDim, ElementSize, PropertyFlags, RepIndex, RepNotifyFunc, BlueprintReplicationCondition);
	}
};

struct FBoolProperty : FProperty {
	uint8	FieldSize;
	uint8	ByteOffset;
	uint8	ByteMask;
	uint8	FieldMask;

	bool read(istream_linker& file) {
		uint8 BoolSize, NativeBool;
		return FProperty::read(file) && iso::read(file, FieldSize, ByteOffset, ByteMask, FieldMask, BoolSize, NativeBool);
	}
};

template<typename T> struct TProperty_Numeric : FProperty {};
typedef TProperty_Numeric<int8>		FInt8Property;
typedef TProperty_Numeric<int16>	FInt16Property;
typedef TProperty_Numeric<int>		FIntProperty;
typedef TProperty_Numeric<int64>	FInt64Property;
typedef TProperty_Numeric<uint8>	FByteProperty;
typedef TProperty_Numeric<uint16>	FUInt16Property;
typedef TProperty_Numeric<uint32>	FUInt32Property;
typedef TProperty_Numeric<uint64>	FUInt64Property;
typedef TProperty_Numeric<float>	FFloatProperty;
typedef TProperty_Numeric<double>	FDoubleProperty;

struct FEnumProperty	: FProperty {
	TInlinePtr<FProperty>	UnderlyingProp;	// The property which represents the underlying type of the enum
	TPtr<UEnum>				Enum;			// The enum represented by this property
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(Enum) && file.read(UnderlyingProp);
	}
};

typedef FProperty	FStrProperty;
typedef FProperty	FNameProperty;

struct FArrayProperty	: FProperty {
	TInlinePtr<FProperty>	Inner;
//	EArrayPropertyFlags		ArrayFlags;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(Inner);
	}
};

struct FSetProperty		: FProperty {
	TInlinePtr<FProperty>	ElementProp;
	//FScriptSet::Layout	SetLayout;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(ElementProp);
	}
};

struct FMapProperty		: FProperty {
	TInlinePtr<FProperty>	KeyProp;
	TInlinePtr<FProperty>	ValueProp;
	//FScriptMap::Layout	MapLayout;
	//EMapPropertyFlags		MapFlags;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(KeyProp) && file.read(ValueProp);
	}
};

struct FStructProperty : FProperty {
	TPtr<UScriptStruct>		Struct;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(Struct);
	}
};

struct FObjectPropertyBase : FProperty {
	TPtr<UClass>			PropertyClass;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(PropertyClass);
	}
};
struct FObjectProperty : FObjectPropertyBase {};

struct FDelegateProperty : FProperty {
	TPtr<UFunction>			SignatureFunction;
	bool read(istream_linker& file) {
		return FProperty::read(file) && file.read(SignatureFunction);
	}
};

ISO_DEFUSER(EPropertyFlags, uint64);
ISO_DEFUSER(ELifetimeCondition, uint8);
ISO_DEFUSERCOMPV(FField, NamePrivate, FlagsPrivate, MetaDataMap);
ISO_DEFUSERCOMPBV(FProperty, FField, ArrayDim,ElementSize,PropertyFlags,RepIndex,RepNotifyFunc,BlueprintReplicationCondition);
ISO_DEFUSERCOMPBV(FObjectProperty, FProperty, PropertyClass);
ISO_DEFUSERCOMPBV(FStructProperty, FProperty, Struct);
ISO_DEFUSERCOMPBV(FBoolProperty, FProperty, FieldSize,ByteOffset,ByteMask,FieldMask);
ISO_DEFUSERCOMPBV(FDelegateProperty, FProperty, SignatureFunction);
ISO_DEFUSERCOMPBV(FArrayProperty, FProperty, Inner);

template<typename T> struct ISO::def<TProperty_Numeric<T>> : ISO::def<FProperty> {};

ObjectLoaderRaw<FObjectProperty>		load_ObjectProperty("ObjectProperty");
ObjectLoaderRaw<FStructProperty>		load_StructProperty("StructProperty");
ObjectLoaderRaw<FDelegateProperty>		load_DelegateProperty("DelegateProperty");
ObjectLoaderRaw<FNameProperty>			load_NameProperty("NameProperty");
ObjectLoaderRaw<FStrProperty>			load_StrProperty("StrProperty");
ObjectLoaderRaw<FArrayProperty>			load_ArrayProperty("ArrayProperty");
ObjectLoaderRaw<FBoolProperty>			load_BoolProperty("BoolProperty");
ObjectLoaderRaw<FInt8Property>			load_Int8Property("Int8Property");
ObjectLoaderRaw<FInt16Property>			load_Int16Propert("Int16Property");
ObjectLoaderRaw<FIntProperty>			load_IntProperty("IntProperty");
ObjectLoaderRaw<FInt64Property>			load_Int64Property("Int64Property");
ObjectLoaderRaw<FByteProperty>			load_ByteProperty("ByteProperty");
ObjectLoaderRaw<FUInt16Property>		load_UInt16Property("UInt16Property");
ObjectLoaderRaw<FUInt32Property>		load_UInt32Property("UInt32Property");
ObjectLoaderRaw<FUInt64Property>		load_UInt64Property("UInt64Property");
ObjectLoaderRaw<FFloatProperty>			load_FloatProperty("FloatProperty");
ObjectLoaderRaw<FDoubleProperty>		load_DoubleProperty("DoubleProperty");

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

struct UObject : anything {
	bool	read(istream_linker& file) {
		ReadTagged(file, *this);
		if (file.get<bool32>())
			Append(ISO::MakePtr("guid", file.get<FGuid>()));
		return true;
	}
};
ISO_DEFUSER(UObject, anything);
ObjectLoaderRaw<UObject>	load_Object("UObject");

struct UStruct : UObject {
	TPtr<UObject>			Super;
	TArray<TPtr<UObject>>	Children;
	TArray<TInlinePtr<FProperty>>	ChildProperties;	//anything				;
	malloc_block			Script;
	
	const UStruct*	GetOuter() const { return this; }
	
	bool read(istream_linker& file) {
		UObject::read(file);
		file.read(Super);
		file.read(Children);
		file.read(ChildProperties);

		auto BytecodeBufferSize			= file.get<int32>();
		auto SerializedScriptSize		= file.get<int32>();
		Script.read(file, SerializedScriptSize);
		return true;
	}
};

ISO_DEFUSERCOMPBV(UStruct, UObject, Super, Children, ChildProperties, Script);
ObjectLoaderRaw<UScriptStruct>	load_Struct("Struct");

struct FImplementedInterface {
	TPtr<UClass>	Class;
	int32			PointerOffset;
	bool32			bImplementedByK2;
	bool read(istream_linker& file) {
		return iso::read(file, Class, PointerOffset, bImplementedByK2);
	}
};

struct UClass : UStruct {
	TMap<FName2, TPtr<UFunction>>	FuncMap;
	EClassFlags						ClassFlags;
	TPtr<UClass>					ClassWithin;
	TPtr<UObject>					ClassGeneratedBy;
	FName2							ClassConfigName;
	TArray<FImplementedInterface>	Interfaces;
	TPtr<UObject> CDO;

	bool read(istream_linker& file) {
		UStruct::read(file);
		iso::read(file, FuncMap, ClassFlags, ClassWithin, ClassConfigName, ClassGeneratedBy, Interfaces);
		file.get<bool32>();//bool bDeprecatedForceScriptOrder = false;
		file.get<FName>();
		if (file.get<bool32>())
			file.read(CDO);
		return true;
	}
};
ISO_DEFUSER(EClassFlags, xint32);
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

struct UTexture2D : UObject {
	ISO_ptr<void>	bitmap;
	bool read(istream_linker& file) {
		UObject::read(file);
		file.seek_cur(2);	//?

		auto	bulk	= file.get<FBulkData>();
		auto	strip	= file.get<FStripDataFlags>();
		auto	cooked	= file.get<bool32>();

		bitmap = FileHandler::Get("png")->Read(0, bulk.reader(file, file.bulk_data()));
		return true;
	}
};
ISO_DEFUSERCOMPBV(UTexture2D, UObject, bitmap);
ObjectLoaderRaw<UTexture2D>	load_Texture2D("Texture2D");


//-----------------------------------------------------------------------------
//	UWorld
//-----------------------------------------------------------------------------

struct UWorld : UObject {
	TPtr<UObject>			PersistentLevel;
	TArray<TPtr<UObject>>	ExtraReferencedObjects;
	TArray<TPtr<UObject>>	StreamingLevels;

	bool	read(istream_linker& file) {
		return UObject::read(file) && file.read(PersistentLevel) && file.read(ExtraReferencedObjects) && file.read(StreamingLevels);
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
			//file.read(CookedFormatData);
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

		//if (bCooked && bProcessCookedData) {
		//	CookedFormatData.Serialize(Ar, this);
		//}

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
	DT_TILE_FREE_DATA = 0x01,	// The navigation mesh owns the tile memory and is responsible for freeing it.
};

/// Flags representing the type of a navigation mesh polygon.
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
		return iso::read(file, vertBase, triBase, vertCount, triCount);
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
		return iso::read(file, startA, startB, endA, endB, rad, firstPoly, npolys, flags, userId);
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
		return iso::read(file, pos, rad, poly, flags, side, userId);
	}
};

struct dtCluster {
	float center[3];				///< Center pos of cluster
	//uint32 firstLink;			///< Link in dtMeshTile.links array
	//uint32 numLinks;			///< Number of cluster links
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
	int		version;			///< Tile data format version number.
	int		x;					///< The x-position of the tile within the dtNavMesh tile grid. (x, y, layer)
	int		y;					///< The y-position of the tile within the dtNavMesh tile grid. (x, y, layer)
	int		layer;				///< The layer of the tile within the dtNavMesh tile grid. (x, y, layer)
	uint32	userId;				///< The user defined id of the tile.
	int		polyCount;			///< The number of polygons in the tile.
	int		vertCount;			///< The number of vertices in the tile.
	int		maxLinkCount;		///< The number of allocated links.
	int		detailMeshCount;	///< The number of sub-meshes in the detail mesh.
	int		detailVertCount;	/// The number of unique vertices in the detail mesh. (In addition to the polygon vertices.)
	int		detailTriCount;		///< The number of triangles in the detail mesh.
	int		bvNodeCount;		///< The number of bounding volume nodes. (Zero if bounding volumes are disabled.)
	int		offMeshConCount;	///< The number of point type off-mesh connections.
	int		offMeshBase;		///< The index of the first polygon which is an point type off-mesh connection.
	float	walkableHeight;		///< The height of the agents using the tile.
	float	walkableRadius;		///< The radius of the agents using the tile.
	float	walkableClimb;		///< The maximum climb height of the agents using the tile.
	float	bmin[3];			///< The minimum bounds of the tile's AABB. [(x, y, z)]
	float	bmax[3];			///< The maximum bounds of the tile's AABB. [(x, y, z)]
	float	bvQuantFactor;		/// The bounding volume quantization factor.
	int		clusterCount;		///< Number of clusters
	int		offMeshSegConCount;	///< The number of segment type off-mesh connections.
	int		offMeshSegPolyBase;	///< The index of the first polygon which is an segment type off-mesh connection
	int		offMeshSegVertBase;	///< The index of the first vertex used by segment type off-mesh connection
};
#if 0
struct dtMeshTile {
	uint32 salt;					///< Counter describing modifications to the tile.
	uint32 linksFreeList;			///< Index to the next free link.
	dtMeshHeader* header;				///< The tile header.
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
	int dataSize;							///< Size of the tile data.
	int flags;								///< Tile flags. (See: #dtTileFlags)
	dtMeshTile* next;						///< The next free tile, or the next tile in the spatial grid.
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
	int maxTiles;					///< The maximum number of tiles the navigation mesh can contain.
	int maxPolys;					///< The maximum number of polygons each tile can contain.
};

struct dtNavMesh {
	static const int DT_MAX_AREAS = 64;

	dtNavMeshParams m_params;			///< Current initialization params. TODO: do not store this info twice.
	float m_orig[3];					///< Origin of the tile (0,0)
	float m_tileWidth, m_tileHeight;	///< Dimensions of each tile.
	int m_maxTiles;						///< Max number of tiles.
	int m_tileLutSize;					///< Tile hash lookup size (must be pot).
	int m_tileLutMask;					///< Tile hash lookup mask.

	uint8 m_areaCostOrder[DT_MAX_AREAS];

	dtMeshTile** m_posLookup;			///< Tile hash lookup.
	dtMeshTile* m_nextFree;				///< Freelist of tiles.
	dtMeshTile* m_tiles;				///< List of tiles.

	uint32 m_saltBits;			///< Number of salt bits in the tile ID.
	uint32 m_tileBits;			///< Number of tile bits in the tile ID.
	uint32 m_polyBits;			///< Number of poly bits in the tile ID.
};
#endif

struct FTileRawData {
	dtMeshHeader							header;
	dynamic_array<FVector>					nav_verts;
	dynamic_array<dtPoly>					nav_polys;
	dynamic_array<dtPolyDetail>				detail_meshes;
	dynamic_array<FVector>					detail_verts;
	dynamic_array<fixed_array<uint8, 4>>	detail_tris;
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

		//if (NavMeshVersion >= NAVMESHVER_OFFMESH_HEIGHT_BUG) {
			for (auto &i : off_mesh_cons) {
				file.read(i.height);
			}
		//}

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
	// shader state that are set at compile time
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

	// wind simulation components that oscillate
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
		// settings
		float		StrengthResponse, DirectionResponse;
		float		AnchorOffset, AnchorDistanceScale;

		// oscillation components
		float		Frequencies[NUM_OSC_COMPONENTS][NUM_WIND_POINTS_IN_CURVE];

		// global motion
		struct {
			float	Height, HeightExponent;
			float	Distance[NUM_WIND_POINTS_IN_CURVE];
			float	DirectionAdherence[NUM_WIND_POINTS_IN_CURVE];
		} Global;

		// branch motion
		struct {
			float	Distance[NUM_WIND_POINTS_IN_CURVE];
			float	DirectionAdherence[NUM_WIND_POINTS_IN_CURVE];
			float	Whip[NUM_WIND_POINTS_IN_CURVE];
			float	Turbulence, Twitch, TwitchFreqScale;
		} Branch[NUM_BRANCH_LEVELS];

		// leaf motion
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

		// frond ripple
		struct {
			float	Distance[NUM_WIND_POINTS_IN_CURVE];
			float	Tile;
			float	LightingScalar;
		} FrondRipple;

		// rolling
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

		// gusting
		struct {
			float	Frequency, StrengthMin, StrengthMax, DurationMin, DurationMax, RiseScalar, FallScalar;
		} Gusting;
	};
	Params		params;
	bool32		options[NUM_WIND_OPTIONS];
	FVector		BranchWindAnchor;
	float		MaxBranchLevel1Length;

	bool	read(istream_ref file) {
		return iso::read(file, params, options, BranchWindAnchor, MaxBranchLevel1Length);
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
		// Engine raw mesh version:
		RAW_MESH_VER_INITIAL = 0,
		RAW_MESH_VER_REMOVE_ZERO_TRIANGLE_SECTIONS,
		// Add new raw mesh versions here.

		RAW_MESH_VER_PLUS_ONE,
		RAW_MESH_VER = RAW_MESH_VER_PLUS_ONE - 1,

		// Licensee raw mesh version:
		RAW_MESH_LIC_VER_INITIAL = 0,
		// Licensees add new raw mesh versions here.

		RAW_MESH_LIC_VER_PLUS_ONE,
		RAW_MESH_LIC_VER = RAW_MESH_LIC_VER_PLUS_ONE - 1
	};

	bool	read(istream_ref file) {
		int32 Version			= RAW_MESH_VER;
		int32 LicenseeVersion	= RAW_MESH_LIC_VER;
		file.read(Version);
		file.read(LicenseeVersion);

		iso::read(file, FaceMaterialIndices, FaceSmoothingMasks, VertexPositions, WedgeIndices, WedgeTangentX,WedgeTangentY,WedgeTangentZ);
		file.read(WedgeTexCoords);
		file.read(WedgeColors);
		file.read(MaterialIndexToImportIndex);
		return true;
	}

};

struct AttributeValuesBase {};

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
			Transient			= 1 << 3	// Attribute is not serialized */
		};
		struct index {
			Attribute	*a;
			int			i;
			template<typename T> operator T*() const {
				auto	type = meta::TL_find<T, AttributeTypes>;
				ISO_ASSERT(a->type == type);
				return (*(static_cast<AttributeValues<T>*>(get(a->arrays))))[i];
			}
		};

		uint32		type;
		unique_ptr<AttributeValuesBase>	arrays;
		uint32		num_elements;
		Flags		flags;

		Attribute() {}
		~Attribute() {}
		Attribute(Attribute&&) = default;
		Attribute& operator=(Attribute&&) = default;

		index	operator[](int i) {
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
		uint32					size;
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
		bool	read(istream_ref file) { return file.read(VertexIDs[0]) && file.read(VertexIDs[1]); }
	};
	struct FMeshTriangle {
		int			VertexInstanceIDs[3];
		int			PolygonID;
		bool	read(istream_ref file) { return file.read(VertexInstanceIDs[0]) && file.read(VertexInstanceIDs[1]) && file.read(VertexInstanceIDs[2]) && file.read(PolygonID); }
	};
	struct FMeshPolygon {
		TArray<int> VertexInstanceIDs;
		TArray<int> TriangleIDs;
		int			PolygonGroupID;
		bool	read(istream_ref file) { return file.read(VertexInstanceIDs) && file.read(PolygonGroupID); }
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
	if (!iso::read(file,
		VertexArray,			VertexInstanceArray,			EdgeArray,			PolygonArray,			PolygonGroupArray,
		VertexAttributesSet,	VertexInstanceAttributesSet,	EdgeAttributesSet,	PolygonAttributesSet,	PolygonGroupAttributesSet,
		TriangleArray,			TriangleAttributesSet
	))
		return false;

	// Populate vertex instance IDs for vertices
	for (auto &i : VertexInstanceArray)
		VertexArray[i->VertexID].VertexInstanceIDs.push_back(VertexInstanceArray.index_of(i));

	// Populate edge IDs for vertices
	for (auto &i : EdgeArray) {
		auto	EdgeID = EdgeArray.index_of(i);
		VertexArray[i->VertexIDs[0]].ConnectedEdgeIDs.push_back(EdgeID);
		VertexArray[i->VertexIDs[1]].ConnectedEdgeIDs.push_back(EdgeID);
	}

	// Make reverse connection from polygons to triangles
	for (auto &i : TriangleArray)
		PolygonArray[i->PolygonID].TriangleIDs.push_back(TriangleArray.index_of(i));

	// Populate polygon IDs for vertex instances, edges and polygon groups
	for (auto &i : PolygonArray) {
		// If the polygon has no contour serialized, copy it over from the triangle
		if (i->VertexInstanceIDs.empty()) {
			ISO_ASSERT(i->TriangleIDs.size() == 1);
			for (auto j : TriangleArray[i->TriangleIDs[0]].VertexInstanceIDs)
				i->VertexInstanceIDs.push_back(j);
		}
		PolygonGroupArray[i->PolygonGroupID].Polygons.push_back(PolygonArray.index_of(i));
	}

	// Otherwise connect existing triangles to vertex instances and edges
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
		(*di)[0]	= i.t.VertexInstanceIDs[2];
		(*di)[1]	= i.t.VertexInstanceIDs[1];
		(*di)[2]	= i.t.VertexInstanceIDs[0];
		++di;
	}

	FVector	*pos	= get(VertexAttributesSet["Position "])[0];
	FVector	*norms	= get(VertexInstanceAttributesSet["Normal"])[0];
	int		nv		= VertexInstanceArray.size();
	auto	dv		= sm->CreateVerts<UnrealVertex>(nv);

	for (auto &i : VertexInstanceArray) {
		int	v = i.t.VertexID;
		dv->pos.x = pos[v].X;
		dv->pos.y = pos[v].Y;
		dv->pos.z = pos[v].Z;

		dv->norm.x = norms[i.i].X;
		dv->norm.y = norms[i.i].Y;
		dv->norm.z = norms[i.i].Z;
		++dv;
	}
	sm->UpdateExtents();
}

struct FRawMeshBulkData : FBulkData {
	FGuid				Guid;
	bool32				bGuidIsHash;
	bool	read(istream_ref file) {
		return FBulkData::read(file) && file.read(Guid) && file.read(bGuidIsHash);
	}
};

struct FMeshUVChannelInfo {
	bool32				bInitialized;
	bool32				bOverrideDensities;
	float				LocalUVDensities[4];
	bool	read(istream_ref file) {
		return iso::read(file, bInitialized, bOverrideDensities, LocalUVDensities);
	}
};

struct FStaticMaterial {
	TPtr<UMaterialInterface>	MaterialInterface;
	FName2						MaterialSlotName;
	FName2						ImportedMaterialSlotName;
	FMeshUVChannelInfo			UVChannelData;
 
	bool	read(istream_linker &file) {
		return iso::read(file, MaterialInterface, MaterialSlotName, ImportedMaterialSlotName, UVChannelData);
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

		file.read(LightingGuid);
		file.read(Sockets);

		if (!StripFlags.IsEditorDataStripped()) {
			auto	SourceModels = ISO::Browser((*this)["SourceModels"]);
			for (auto &&i : SourceModels) {
				bool bIsValid = file.get<bool32>();
				if (bIsValid) {
					auto	bulk	= file.get<FRawMeshBulkData>();
					auto	pos		= make_save_pos(file);
					//RawMeshData.push_back(malloc_block::unterminated(bulk.reader(file, file.bulk_data())));
					MeshDescriptions.push_back().read(bulk.reader(file, file.bulk_data()));
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
			//file.read(SpeedTreeWind);
			file.get<FSpeedTreeWind>();
			return true;
		}

		file.read(StaticMaterials);

		return true;
	}
};

ISO_DEFUSERCOMPV(FMeshDescription::Attribute, type, num_elements);
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
		file.read(PerInstanceSMData);
		file.read(PerInstanceSMCustomData);
		return true;
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
		return iso::read(file, BasePtr, CachedLocation, CachedRotation, CachedDrawScale);
	}
};
struct FFoliageInstanceBaseCache {
	typedef int32 FFoliageInstanceBaseId;
	FFoliageInstanceBaseId		NextBaseId;
	TMap<FFoliageInstanceBaseId, FFoliageInstanceBaseInfo>	InstanceBaseMap;
	TMap<TPtr<UWorld>, TArray<TPtr<UActorComponent>>>		InstanceBaseLevelMap;
	bool	read(istream_linker& file) {
		return iso::read(file, NextBaseId, InstanceBaseMap, InstanceBaseLevelMap);
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
		//bool read(istream_ref &file) {
		//	return iso::read(file, iVertex, UVs, Color);
		//}
	};
	struct FMeshFace {
		uint32			iWedge[3];
		uint16			MeshMaterialIndex;
		FVector			TangentX[3], TangentY[3], TangentZ[3];
		uint32			SmoothingGroups;
	};
	struct FJointPos {
		FTransform	Transform;
		//bool read(istream_ref &file) {
		//	return file.read(Transform);
		//}
	};
	struct FTriangle {
		uint8			MatIndex;
		uint8			AuxMatIndex;
		uint32			SmoothingGroups;
		uint32			WedgeIndex[3];
		FVector			TangentX[3], TangentY[3], TangentZ[3];
		//bool read(istream_ref &file) {
		//	return iso::read(file, MatIndex, AuxMatIndex, SmoothingGroups, WedgeIndex, TangentX, TangentY, TangentZ);
		//}
	};
	struct FVertInfluence {
		float			Weight;
		uint32			VertIndex;
		FBoneIndexType	BoneIndex;
		//bool read(istream_ref &file) {
		//	return iso::read(file, Weight, VertIndex, BoneIndex);
		//}
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
			return iso::read(file, Name, Flags, NumChildren, ParentIndex, BonePos);
		}
	};
	struct FRawBoneInfluence {
		float		Weight;
		int32		VertexIndex;
		int32		BoneIndex;
		//bool read(istream_ref &file) {
		//	return iso::read(file, Weight, VertexIndex, BoneIndex);
		//}
	};
	struct FVertex {
		uint32		VertexIndex;	// Index to a vertex.
		FVector2D	UVs[4];		// Scaled to BYTES, rather...-> Done in digestion phase, on-disk size doesn't matter here.
		FColor		Color;			// Vertex colors
		uint8		MatIndex;		// At runtime, this one will be implied by the face that's pointing to us.
		uint8		Reserved;		// Top secret.
		FVertex() { clear(*this); }
		bool read(istream_ref &file) {
			return iso::read(file, VertexIndex, Color, MatIndex, Reserved, UVs);
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

	// Alternate influence imported(i.e. FBX) data. The name is the alternate skinning profile name
	TArray<FSkeletalMeshImportData> AlternateInfluences;
	TArray<FString>					AlternateInfluenceProfileNames;

	bool	read(istream_ref file) {
		int32 Version			= file.get();
		int32 LicenseeVersion	= file.get();
		iso::read(file, bDiffPose, bHasNormals, bHasTangents, bHasVertexColors, bUseT0AsRefPose, MaxMaterialIndex, NumTexCoords, Faces, Influences, Materials, Points, PointToRawMap, RefBonesBinary, Wedges);
		return iso::read(file, MorphTargets, MorphTargetModifiedPoints, MorphTargetNames, AlternateInfluences, AlternateInfluenceProfileNames);
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
		return iso::read(file, GeoImportVersion, SkinningImportVersion, BulkData, Guid, bGuidIsHash);
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
			//RawSkeletalMeshBulkDatas.push_back(malloc_block::unterminated(data.BulkData.reader(file, file.bulk_data())));
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
		return iso::read(file, Prob, Alias, TotalWeight);
	}
};

struct FSkeletalMeshAreaWeightedTriangleSampler : FWeightedRandomSampler {
	//USkeletalMesh* Owner;
	//TArray<int32>*	TriangleIndices;
	//int32			LODIndex;
};

struct FSkeletalMeshSamplingLODBuiltData {
	FSkeletalMeshAreaWeightedTriangleSampler AreaWeightedTriangleSampler;
	bool	read(istream_linker& file) {
		return file.read(AreaWeightedTriangleSampler);
	}
};
ObjectLoaderRaw<FSkeletalMeshSamplingLODBuiltData>	load_SkeletalMeshSamplingLODBuiltData("SkeletalMeshSamplingLODBuiltData");

struct FMeshBoneInfo {
	FName2	Name;
	int32	ParentIndex;
	FString ExportName;
	bool	read(istream_linker& file) {
		return iso::read(file, Name, ParentIndex, ExportName);
	}
};

struct FReferenceSkeleton {
	TArray<FMeshBoneInfo>	RefBoneInfo;
	TArray<FTransform>		RefBonePose;
	TMap<FName2, int32>		NameToIndexMap;
	bool	read(istream_linker& file) {
		return iso::read(file, RefBoneInfo, RefBonePose, NameToIndexMap);
	}
};

struct FClothingSectionData {
	FGuid	AssetGuid;
	int32	AssetLodIndex;
	bool	read(istream_linker& file) {
		return file.read(AssetGuid) && file.read(AssetLodIndex);
	}
};
struct FMeshToMeshVertData {
	FVector4 PositionBaryCoordsAndDist;
	FVector4 NormalBaryCoordsAndDist;
	FVector4 TangentBaryCoordsAndDist;
	uint16	 SourceMeshVertIndices[4];
	uint32	 Padding[2];
	//bool	read(istream_linker& file) {
	//	return iso::read(file, PositionBaryCoordsAndDist, NormalBaryCoordsAndDist, TangentBaryCoordsAndDist, SourceMeshVertIndices, Padding);
	//}
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
		return iso::read(file, Position, TangentX, TangentY, TangentZ, UVs, Color, InfluenceBones, InfluenceWeights);
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

		return iso::read(file, bUse16BitBoneIndex, BoneMap, NumVertices, MaxBoneInfluences, ClothMappingData, CorrespondClothAssetIndex, ClothingData, OverlappingVertices, bDisabled, GenerateUpToLodIndex, OriginalDataSectionIndex, ChunkedParentSectionIndex);
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
		sm->UpdateExtents();
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
		return StripFlags.IsEditorDataStripped() || iso::read(file, bRecomputeTangent, bCastShadow, bDisabled, GenerateUpToLodIndex, CorrespondClothAssetIndex, ClothingData);
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
	bool	read(istream_linker& file) { return iso::read(file, Weight, VertIndex, BoneIndex); }
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

		// no longer in use
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

		//@todo legacy
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
		return iso::read(file, Type, LinkedBones, MaxLOD);
	}
};

struct FSmartNameMapping {
	TMap<FName, FCurveMetaData> CurveMetaDataMap;
	bool	read(istream_linker& file) {
		return iso::read(file, CurveMetaDataMap);
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
	int32		iCollisionBound;// 4  Collision bound.
	uint8		iZone[2];		// 2  Visibility zone in 1=front, 0=back.
	uint8		NumVertices;	// 1  Number of vertices in node.
	uint8		NodeFlags;		// 1  Node flags.
	int32		iLeaf[2];		// 8  Leaf in back and front, INDEX_NONE=not a leaf.
};

struct FBspSurf {
	TPtr<UMaterialInterface>	Material;		// 4 Material.
	uint32				PolyFlags;		// 4 Polygon flags.
	int32				pBase;			// 4 Polygon & texture base point index (where U,V==0,0).
	int32				vNormal;		// 4 Index to polygon normal.
	int32				vTextureU;		// 4 Texture U-vector index.
	int32				vTextureV;		// 4 Texture V-vector index.
	int32				iBrushPoly;		// 4 Editor brush polygon index.
	TPtr<ABrush>		Actor;			// 4 Brush actor owning this Bsp surface.
	FPlane				Plane;			// 16 The plane this surface lies on.
	float				LightMapScale;	// 4 The number of units/lightmap texel on this surface.
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
	int32 		pVertex;	// Index of vertex.
	int32		iSide;		// If shared, index of unique side. Otherwise INDEX_NONE.
	FVector2D	ShadowTexCoord;
	FVector2D	BackfaceShadowTexCoord;
};

struct FPoly {
	FVector				Base;					// Base point of polygon.
	FVector				Normal;					// Normal of polygon.
	FVector				TextureU;				// Texture U vector.
	FVector				TextureV;				// Texture V vector.
	TArray<FVector>		Vertices;
	uint32				PolyFlags;				// FPoly & Bsp poly bit flags (PF_).
	TPtr<ABrush>				Actor;					// Brush where this originated, or NULL.
	TPtr<UMaterialInterface>	Material;
	FName				RulesetVariation;		// Name of variation within a ProcBuilding Ruleset for this face
	FName				ItemName;				// Item name.
	int32				iLink;					// iBspSurf, or brush fpoly index of first identical polygon, or MAX_uint16.
	int32				iLinkSurf;
	int32				iBrushPoly;				// Index of editor solid's polygon this originated from.
	uint32				SmoothingMask;			// A mask used to determine which smoothing groups this polygon is in.  SmoothingMask & (1 << GroupNumber)
	float				LightMapScale;			// The number of units/shadowmap texel on this surface.
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
		return iso::read(file,  MapBuildDataId, Component, Material, Nodes);
	}
};

struct UModelComponent : UObject {
	TPtr<UModel>	Model;
	int32			ComponentIndex;
	TArray<uint16>	Nodes;
	TArray<FModelElement> Elements;
	bool	read(istream_linker& file) {
		return UObject::read(file) && iso::read(file, Model, Elements, ComponentIndex, Nodes);
	}
};

ISO_DEFUSERCOMPV(FModelElement,		Component, Material, Nodes, MapBuildDataId);
ISO_DEFUSERCOMPBV(UModelComponent,	UObject, Model, ComponentIndex, Nodes, Elements);
ObjectLoaderRaw<UModelComponent>	load_ModelComponent("ModelComponent");

//-----------------------------------------------------------------------------
//	sound
//-----------------------------------------------------------------------------

struct USoundWave : UObject {
	ISO_ptr<void>	sample;
	FGuid			CompressedDataGuid;

	bool read(istream_linker &file) {
		UObject::read(file);
		auto	cooked	= file.get<bool32>();
		auto	bulk	= file.get<FBulkData>();
		sample = FileHandler::Get("wav")->Read(0, bulk.reader(file, file.bulk_data()));
		return file.read(CompressedDataGuid);
	}
};
ISO_DEFUSERCOMPBV(USoundWave,	UObject, sample, CompressedDataGuid);
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
		return iso::read(file, 
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
		return iso::read(file, MemberParent, MemberName, MemberGuid);
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
		iso::read(file, PinCategory, PinSubCategory, PinSubCategoryObject, ContainerType);
		if (ContainerType == EPinContainerType::Map)
			file.read(PinValueType);

		return iso::read(file, bIsReference, bIsWeakPointer, PinSubCategoryMemberReference, bIsConst);
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
		return iso::read(file, bCompressed, UncompressedSize, Data);
	}
};

struct FPrecomputedVisibilityBucket {
	int32								CellDataSize;
	TArray<FPrecomputedVisibilityCell>	Cells;
	TArray<FCompressedVisibilityChunk>	CellDataChunks;
	bool read(istream_ref file) {
		return iso::read(file, CellDataSize, Cells, CellDataChunks);
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
		return iso::read(file, 
			VolumeMaxDistance,
			VolumeBox,
			VolumeSizeX,
			VolumeSizeY,
			VolumeSizeZ,
			Data
		);
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
			&& iso::read(file, Actors, URL, Model, ModelComponents, LevelScriptBlueprint, NavListStart, NavListEnd)
			;//&& iso::read(PrecomputedVisibilityHandler, PrecomputedVolumeDistanceField);
	}
};

ISO_DEFUSERCOMPV(FPrecomputedVisibilityCell, Min, ChunkIndex, DataOffset);
ISO_DEFUSERCOMPV(FCompressedVisibilityChunk, bCompressed, UncompressedSize, Data);
ISO_DEFUSERCOMPV(FPrecomputedVisibilityBucket, CellDataSize, Cells, CellDataChunks);
ISO_DEFUSERCOMPV(FPrecomputedVisibilityHandler, CellBucketOriginXY, CellSizeXY, CellSizeZ, CellBucketSizeXY, NumCellBuckets, CellBuckets);
ISO_DEFUSERCOMPV(FPrecomputedVolumeDistanceField, VolumeMaxDistance, VolumeBox, VolumeSizeX, VolumeSizeY, VolumeSizeZ, Data);
ISO_DEFUSERCOMPBV(ULevel, UObject, Actors, URL, Model, ModelComponents, LevelScriptBlueprint, NavListStart, NavListEnd, PrecomputedVisibilityHandler, PrecomputedVolumeDistanceField);

ObjectLoaderRaw<ULevel>	load_Level("Level");

//-----------------------------------------------------------------------------
//	function
//-----------------------------------------------------------------------------

enum EFunctionFlags : uint32 {
	// Function flags.
	FUNC_None				= 0x00000000,

	FUNC_Final				= 0x00000001,	// Function is final (prebindable, non-overridable function).
	FUNC_RequiredAPI		= 0x00000002,	// Indicates this function is DLL exported/imported.
	FUNC_BlueprintAuthorityOnly= 0x00000004,   // Function will only run if the object has network authority
	FUNC_BlueprintCosmetic	= 0x00000008,   // Function is cosmetic in nature and should not be invoked on dedicated servers
	// FUNC_				= 0x00000010,   // unused.
	// FUNC_				= 0x00000020,   // unused.
	FUNC_Net				= 0x00000040,   // Function is network-replicated.
	FUNC_NetReliable		= 0x00000080,   // Function should be sent reliably on the network.
	FUNC_NetRequest			= 0x00000100,	// Function is sent to a net service
	FUNC_Exec				= 0x00000200,	// Executable from command line.
	FUNC_Native				= 0x00000400,	// Native function.
	FUNC_Event				= 0x00000800,   // Event function.
	FUNC_NetResponse		= 0x00001000,   // Function response from a net service
	FUNC_Static				= 0x00002000,   // Static function.
	FUNC_NetMulticast		= 0x00004000,	// Function is networked multicast Server -> All Clients
	FUNC_UbergraphFunction	= 0x00008000,   // Function is used as the merge 'ubergraph' for a blueprint, only assigned when using the persistent 'ubergraph' frame
	FUNC_MulticastDelegate	= 0x00010000,	// Function is a multi-cast delegate signature (also requires FUNC_Delegate to be set!)
	FUNC_Public				= 0x00020000,	// Function is accessible in all classes (if overridden, parameters must remain unchanged).
	FUNC_Private			= 0x00040000,	// Function is accessible only in the class it is defined in (cannot be overridden, but function name may be reused in subclasses.  IOW: if overridden, parameters don't need to match, and Super.Func() cannot be accessed since it's private.)
	FUNC_Protected			= 0x00080000,	// Function is accessible only in the class it is defined in and subclasses (if overridden, parameters much remain unchanged).
	FUNC_Delegate			= 0x00100000,	// Function is delegate signature (either single-cast or multi-cast, depending on whether FUNC_MulticastDelegate is set.)
	FUNC_NetServer			= 0x00200000,	// Function is executed on servers (set by replication code if passes check)
	FUNC_HasOutParms		= 0x00400000,	// function has out (pass by reference) parameters
	FUNC_HasDefaults		= 0x00800000,	// function has structs that contain defaults
	FUNC_NetClient			= 0x01000000,	// function is executed on clients
	FUNC_DLLImport			= 0x02000000,	// function is imported from a DLL
	FUNC_BlueprintCallable	= 0x04000000,	// function can be called from blueprint code
	FUNC_BlueprintEvent		= 0x08000000,	// function can be overridden/implemented from a blueprint
	FUNC_BlueprintPure		= 0x10000000,	// function can be called from blueprint code, and is also pure (produces no side effects). If you set this, you should set FUNC_BlueprintCallable as well.
	FUNC_EditorOnly			= 0x20000000,	// function can only be called from an editor scrippt.
	FUNC_Const				= 0x40000000,	// function can be called from blueprint code, and only reads state (never writes state)
	FUNC_NetValidate		= 0x80000000,	// function must supply a _Validate implementation

	FUNC_AllFlags			= 0xFFFFFFFF,

	// Combinations of flags
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
			// Unused
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
		return iso::read(file, OutputIndex, InputName, Mask, MaskR, MaskG, MaskB, MaskA);
	}
};
ISO_DEFUSERCOMPV(FExpressionInput, OutputIndex, InputName, Mask, MaskR, MaskG, MaskB, MaskA);
ObjectLoaderRaw<FExpressionInput>	load_ExpressionInput("ExpressionInput");

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

ISO_DEFUSER(FString,char*);
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

// Load the names and file offsets for the thumbnails in this package
void LoadThumbnails(istream_ref file, anything &thumbs) {
	int32 n = file.get<int>();
	for (int32 i = 0; i < n; ++i) {
		FString	ObjectClassName					= file.get();
		FString	ObjectPathWithoutPackageName	= file.get();
		int32	FileOffset						= file.get();

		if (ObjectClassName && ObjectClassName != cstr("???")) {
			auto		fp = make_save_pos(file, FileOffset);
			int32		width, height, size;
			read(file, width, height, size);
			thumbs.Append(FileHandler::Get("png")->Read((const char*)ObjectClassName, file));
		}
	}
}

//-----------------------------------------------------------------------------
// unreal bytecode
//-----------------------------------------------------------------------------

enum { MAX_STRING_CONST_SIZE = 1024, MAX_SIMPLE_RETURN_VALUE_SIZE = 64 };

typedef uint16 VariableSizeType;
typedef uint32 CodeSkipSizeType;


// Evaluatable expression item types
enum EExprToken : uint8 {
	// Variable references.
	EX_LocalVariable		= 0x00,	// A local variable.
	EX_InstanceVariable		= 0x01,	// An object variable.
	EX_DefaultVariable		= 0x02, // Default variable for a class context.
	//						= 0x03,
	EX_Return				= 0x04,	// Return from function.
	//						= 0x05,
	EX_Jump					= 0x06,	// Goto a local address in code.
	EX_JumpIfNot			= 0x07,	// Goto if not expression.
	//						= 0x08,
	EX_Assert				= 0x09,	// Assertion.
	//						= 0x0A,
	EX_Nothing				= 0x0B,	// No operation.
	//						= 0x0C,
	//						= 0x0D,
	//						= 0x0E,
	EX_Let					= 0x0F,	// Assign an arbitrary size value to a variable.
	//						= 0x10,
	//						= 0x11,
	EX_ClassContext			= 0x12,	// Class default object context.
	EX_MetaCast             = 0x13, // Metaclass cast.
	EX_LetBool				= 0x14, // Let boolean variable.
	EX_EndParmValue			= 0x15,	// end of default value for optional function parameter
	EX_EndFunctionParms		= 0x16,	// End of function call parameters.
	EX_Self					= 0x17,	// Self object.
	EX_Skip					= 0x18,	// Skippable expression.
	EX_Context				= 0x19,	// Call a function through an object context.
	EX_Context_FailSilent	= 0x1A, // Call a function through an object context (can fail silently if the context is NULL; only generated for functions that don't have output or return values).
	EX_VirtualFunction		= 0x1B,	// A function call with parameters.
	EX_FinalFunction		= 0x1C,	// A prebound function call with parameters.
	EX_IntConst				= 0x1D,	// Int constant.
	EX_FloatConst			= 0x1E,	// Floating point constant.
	EX_StringConst			= 0x1F,	// String constant.
	EX_ObjectConst		    = 0x20,	// An object constant.
	EX_NameConst			= 0x21,	// A name constant.
	EX_RotationConst		= 0x22,	// A rotation constant.
	EX_VectorConst			= 0x23,	// A vector constant.
	EX_ByteConst			= 0x24,	// A byte constant.
	EX_IntZero				= 0x25,	// Zero.
	EX_IntOne				= 0x26,	// One.
	EX_True					= 0x27,	// Bool True.
	EX_False				= 0x28,	// Bool False.
	EX_TextConst			= 0x29, // FText constant
	EX_NoObject				= 0x2A,	// NoObject.
	EX_TransformConst		= 0x2B, // A transform constant
	EX_IntConstByte			= 0x2C,	// Int constant that requires 1 byte.
	EX_NoInterface			= 0x2D, // A null interface (similar to EX_NoObject, but for interfaces)
	EX_DynamicCast			= 0x2E,	// Safe dynamic class casting.
	EX_StructConst			= 0x2F, // An arbitrary UStruct constant
	EX_EndStructConst		= 0x30, // End of UStruct constant
	EX_SetArray				= 0x31, // Set the value of arbitrary array
	EX_EndArray				= 0x32,
	//						= 0x33,
	EX_UnicodeStringConst   = 0x34, // Unicode string constant.
	EX_Int64Const			= 0x35,	// 64-bit integer constant.
	EX_UInt64Const			= 0x36,	// 64-bit unsigned integer constant.
	//						= 0x37,
	EX_PrimitiveCast		= 0x38,	// A casting operator for primitives which reads the type as the subsequent byte
	EX_SetSet				= 0x39,
	EX_EndSet				= 0x3A,
	EX_SetMap				= 0x3B,
	EX_EndMap				= 0x3C,
	EX_SetConst				= 0x3D,
	EX_EndSetConst			= 0x3E,
	EX_MapConst				= 0x3F,
	EX_EndMapConst			= 0x40,
	//						= 0x41,
	EX_StructMemberContext	= 0x42, // Context expression to address a property within a struct
	EX_LetMulticastDelegate	= 0x43, // Assignment to a multi-cast delegate
	EX_LetDelegate			= 0x44, // Assignment to a delegate
	EX_LocalVirtualFunction	= 0x45, // Special instructions to quickly call a virtual function that we know is going to run only locally
	EX_LocalFinalFunction	= 0x46, // Special instructions to quickly call a final function that we know is going to run only locally
	//						= 0x47, // CST_ObjectToBool
	EX_LocalOutVariable		= 0x48, // local out (pass by reference) function parameter
	//						= 0x49, // CST_InterfaceToBool
	EX_DeprecatedOp4A		= 0x4A,
	EX_InstanceDelegate		= 0x4B,	// const reference to a delegate or normal function object
	EX_PushExecutionFlow	= 0x4C, // push an address on to the execution flow stack for future execution when a EX_PopExecutionFlow is executed.   Execution continues on normally and doesn't change to the pushed address.
	EX_PopExecutionFlow		= 0x4D, // continue execution at the last address previously pushed onto the execution flow stack.
	EX_ComputedJump			= 0x4E,	// Goto a local address in code, specified by an integer value.
	EX_PopExecutionFlowIfNot = 0x4F, // continue execution at the last address previously pushed onto the execution flow stack, if the condition is not true.
	EX_Breakpoint			= 0x50, // Breakpoint.  Only observed in the editor, otherwise it behaves like EX_Nothing.
	EX_InterfaceContext		= 0x51,	// Call a function through a native interface variable
	EX_ObjToInterfaceCast   = 0x52,	// Converting an object reference to native interface variable
	EX_EndOfScript			= 0x53, // Last byte in script code
	EX_CrossInterfaceCast	= 0x54, // Converting an interface variable reference to native interface variable
	EX_InterfaceToObjCast   = 0x55, // Converting an interface variable reference to an object
	//						= 0x56,
	//						= 0x57,
	//						= 0x58,
	//						= 0x59,
	EX_WireTracepoint		= 0x5A, // Trace point.  Only observed in the editor, otherwise it behaves like EX_Nothing.
	EX_SkipOffsetConst		= 0x5B, // A CodeSizeSkipOffset constant
	EX_AddMulticastDelegate = 0x5C, // Adds a delegate to a multicast delegate's targets
	EX_ClearMulticastDelegate = 0x5D, // Clears all delegates in a multicast target
	EX_Tracepoint			= 0x5E, // Trace point.  Only observed in the editor, otherwise it behaves like EX_Nothing.
	EX_LetObj				= 0x5F,	// assign to any object ref pointer
	EX_LetWeakObjPtr		= 0x60, // assign to a weak object pointer
	EX_BindDelegate			= 0x61, // bind object and name to delegate
	EX_RemoveMulticastDelegate = 0x62, // Remove a delegate from a multicast delegate's targets
	EX_CallMulticastDelegate = 0x63, // Call multicast delegate
	EX_LetValueOnPersistentFrame = 0x64,
	EX_ArrayConst			= 0x65,
	EX_EndArrayConst		= 0x66,
	EX_SoftObjectConst		= 0x67,
	EX_CallMath				= 0x68, // static pure function from on local call space
	EX_SwitchValue			= 0x69,
	EX_InstrumentationEvent	= 0x6A, // Instrumentation event
	EX_ArrayGetByRef		= 0x6B,
	EX_ClassSparseDataVariable = 0x6C, // Sparse data variable
	EX_FieldPathConst		= 0x6D,
};

enum ECastToken {
	CST_ObjectToInterface	= 0x46,
	CST_ObjectToBool		= 0x47,
	CST_InterfaceToBool		= 0x49,
	CST_Max					= 0xFF,
};

// Kinds of text literals
enum class EBlueprintTextLiteralType : uint8 {
	Empty,				// Text is an empty string. The bytecode contains no strings, and you should use FText::GetEmpty() to initialize the FText instance
	LocalizedText,		// Text is localized. The bytecode will contain three strings - source, key, and namespace - and should be loaded via FInternationalization
	InvariantText,		// Text is culture invariant. The bytecode will contain one string, and you should use FText::AsCultureInvariant to initialize the FText instance
	LiteralString,		// Text is a literal FString. The bytecode will contain one string, and you should use FText::FromString to initialize the FText instance
	StringTableEntry,	// Text is from a string table. The bytecode will contain an object pointer (not used) and two strings - the table ID, and key - and should be found via FText::FromStringTable
};

struct FFrame;

// Information about a blueprint instrumentation signal
struct FScriptInstrumentationSignal {
	enum Type {
		Class = 0,
		ClassScope,
		Instance,
		Event,
		InlineEvent,
		ResumeEvent,
		PureNodeEntry,
		NodeDebugSite,
		NodeEntry,
		NodeExit,
		PushState,
		RestoreState,
		ResetState,
		SuspendState,
		PopState,
		TunnelEndOfThread,
		Stop
	};
	Type EventType;	// The event signal type
	const UObject*			ContextObject;	// The context object the event is from
	const UFunction*		Function;		// The function that emitted this event
	const FName				EventName;		// The event override name
	const FFrame*			StackFramePtr;	// The stack frame for the
	const int32				LatentLinkId;

	FScriptInstrumentationSignal(Type InEventType, const UObject* InContextObject, const struct FFrame& InStackFrame, const FName EventNameIn = {});

	FScriptInstrumentationSignal(Type InEventType, const UObject* InContextObject, UFunction* InFunction, const int32 LinkId = -1)
		: EventType(InEventType)
		, ContextObject(InContextObject)
		, Function(InFunction)
		, StackFramePtr(nullptr)
		, LatentLinkId(LinkId)
	{}

	void			SetType(Type InType)			{ EventType = InType;	}

	Type			GetType()				const	{ return EventType; }
	bool			IsContextObjectValid()	const	{ return ContextObject != nullptr; }
	const UObject*	GetContextObject()		const	{ return ContextObject; }
	bool IsStackFrameValid()				const	{ return StackFramePtr != nullptr; }
	const FFrame&	GetStackFrame()			const	{ return *StackFramePtr; }
	const UClass*	GetClass()				const;
	const UClass*	GetFunctionClassScope() const;
	FName			GetFunctionName()		const;
	int32			GetScriptCodeOffset()	const;
	int32			GetLatentLinkId()		const	{ return LatentLinkId; }
};

struct FScriptName {
	uint32	ComparisonIndex;
	uint32	DisplayIndex;
	uint32	Number;
};

class DisassemblerUnreal : public Disassembler {

	struct State : Disassembler::State {
		struct Line {
			uint64	offset;
			uint32	indent;
			string	dis;
			Line(uint64 offset, uint32 indent, const char *dis) : offset(offset), indent(indent), dis(dis) {}
		};
		dynamic_array<Line>		lines;

		FLinkerTables	*linker	= nullptr;
		int				indent	= 0;
		memory_reader	reader;

		uint64			op_start;

		cstring			ReadName() {
#if 0
			auto	name = reader.get<FScriptName>();
			return linker ? linker->lookup(FName(name.ComparisonIndex, name.Number)) : "????";
#else
			auto	name = reader.get<FName>();
			return linker ? linker->lookup(name) : "????";
#endif
		}
		const char*		GetName(const UObject* obj) {
			return (*obj)["NamePrivate"];
		}
		const char*		GetNameSafe(const UObject* obj) {
			if (obj)
				return GetName(obj);
			return "none";
		}
		const char*		GetName(const FField* field) {
			return (const char*)field->NamePrivate;
		}
		const char*		GetNameSafe(const FField* field) {
			if (field)
				return (const char*)field->NamePrivate;
			return "none";
		}

		const char*	GetFullName(const UObject* obj) {
			return GetName(obj);
		}

		FString		ReadString8()	{ string	s; read(reader, s); return s.begin(); }
		FString		ReadString16()	{ string16	s; read(reader, s); return s.begin(); }
		FString		ReadString() {
			switch (reader.get<EExprToken>()) {
				case EX_StringConst:
					return ReadString8();
				case EX_UnicodeStringConst:
					return ReadString16();
				default:
					ISO_ASSERT(0);
					break;
			}
			return FString();
		}

		auto		Read(FField* p) {
			auto	path	= reader.get<TArray<FName>>();
			auto	xtra	= reader.get<uint32>();
			return p;
		}

		auto		Read(UObject* p) {
			auto	i = reader.get<FPackageIndex>();
			return p;
		}

		//template<typename T> T* ReadPointer() { return (T*)reader.get<uint64>(); }
		template<typename T> const T* ReadPointer() { 
			static	T	dummy;
			return (T*)Read(&dummy);
		}

		EExprToken	SerializeExpr();
		void		ProcessCommon(EExprToken Opcode);

		void		AddIndent()		{ ++indent; }
		void		DropIndent()	{ --indent; }

		void		output(const char *fmt, ...) {
			va_list valist;
			va_start(valist, fmt);
			buffer_accum<256>	ba;
			ba.vformat(fmt, valist) << '\n';
			lines.emplace_back(op_start, indent, (const char*)ba);
		}

		int		Count()												{ return lines.size32(); }
		void	GetLine(string_accum &a, int i, int, SymbolFinder)	{
			if (i < lines.size32())
				a << formatted(lines[i].offset, FORMAT::HEX | FORMAT::ZEROES, 4) << repeat("    ", lines[i].indent) << lines[i].dis;
		}
		uint64	GetAddress(int i)									{ return lines[min(i, lines.size32() - 1)].offset; }

		State(const memory_block &block) : reader(block) {}
	};

public:
	const char*	GetDescription() override { return "unreal bytecode"; }
	Disassembler::State*		Disassemble(const memory_block &block, uint64 addr, SymbolFinder sym_finder) override;
} dis_unreal;


Disassembler::State *DisassemblerUnreal::Disassemble(const memory_block &block, uint64 addr, SymbolFinder sym_finder) {
	State	*state = new State(block);

	while (!state->reader.eof()) {
		//output("Label_0x%X:");
		state->AddIndent();
		state->SerializeExpr();
		state->DropIndent();
	}
	return state;
}

EExprToken DisassemblerUnreal::State::SerializeExpr() {
	AddIndent();
	EExprToken Opcode = reader.get<EExprToken>();
	ProcessCommon(Opcode);
	DropIndent();
	return Opcode;
}

void DisassemblerUnreal::State::ProcessCommon(EExprToken Opcode) {
	op_start = reader.tell();

	switch (Opcode) {
		case EX_PrimitiveCast: {
			uint8 ConversionType = reader.get<uint8>();
			output("$%X: PrimitiveCast of type %d", (int32)Opcode, ConversionType);
			AddIndent();

			output("Argument:");
			//ProcessCastByte(ConversionType);
			SerializeExpr();

			//@TODO:
			// output("Expression:");
			// SerializeExpr(  );
			DropIndent();

			break;
		}
		case EX_SetSet: {
			output("$%X: set set", (int32)Opcode);
			SerializeExpr();
			reader.get<int>();
			while (SerializeExpr() != EX_EndSet) {}
			break;
		}
		case EX_EndSet: {
			output("$%X: EX_EndSet", (int32)Opcode);
			break;
		}
		case EX_SetConst: {
			auto	InnerProp	= ReadPointer<FProperty>();
			int32	Num			= reader.get<int>();
			output("$%X: set set const - elements number: %d, inner property: %s", (int32)Opcode, Num, GetNameSafe(InnerProp));
			while (SerializeExpr() != EX_EndSetConst) {}
			break;
		}
		case EX_EndSetConst: {
			output("$%X: EX_EndSetConst", (int32)Opcode);
			break;
		}
		case EX_SetMap: {
			output("$%X: set map", (int32)Opcode);
			SerializeExpr();
			reader.get<int>();
			while (SerializeExpr() != EX_EndMap) {}
			break;
		}
		case EX_EndMap: {
			output("$%X: EX_EndMap", (int32)Opcode);
			break;
		}
		case EX_MapConst: {
			auto KeyProp = ReadPointer<FProperty>();
			auto ValProp = ReadPointer<FProperty>();
			int32	   Num	   = reader.get<int>();
			output("$%X: set map const - elements number: %d, key property: %s, val property: %s", (int32)Opcode, Num, GetNameSafe(KeyProp), GetNameSafe(ValProp));
			while (SerializeExpr() != EX_EndMapConst) {
				// Map contents
			}
			break;
		}
		case EX_EndMapConst: {
			output("$%X: EX_EndMapConst", (int32)Opcode);
			break;
		}
		case EX_ObjToInterfaceCast: {
			// A conversion from an object variable to a native interface variable
			auto InterfaceClass = ReadPointer<UClass>();
			output("$%X: ObjToInterfaceCast to %s", (int32)Opcode, GetName(InterfaceClass));
			SerializeExpr();
			break;
		}
		case EX_CrossInterfaceCast: {
			// A conversion from one interface variable to a different interface variable
			auto InterfaceClass = ReadPointer<UClass>();
			output("$%X: InterfaceToInterfaceCast to %s", (int32)Opcode, GetName(InterfaceClass));
			SerializeExpr();
			break;
		}
		case EX_InterfaceToObjCast: {
			// A conversion from an interface variable to a object variable
			auto ObjectClass = ReadPointer<UClass>();
			output("$%X: InterfaceToObjCast to %s", (int32)Opcode, GetName(ObjectClass));
			SerializeExpr();
			break;
		}
		case EX_Let: {
			output("$%X: Let (Variable = Expression)", (int32)Opcode);
			AddIndent();
			ReadPointer<FProperty>();
			output("Variable:");	SerializeExpr();
			output("Expression:");	SerializeExpr();
			DropIndent();
			break;
		}
		case EX_LetObj:
		case EX_LetWeakObjPtr: {
			if (Opcode == EX_LetObj)
				output("$%X: Let Obj (Variable = Expression)", (int32)Opcode);
			else
				output("$%X: Let WeakObjPtr (Variable = Expression)", (int32)Opcode);
			AddIndent();
			output("Variable:");	SerializeExpr();
			output("Expression:");	SerializeExpr();
			DropIndent();
			break;
		}
		case EX_LetBool: {
			output("$%X: LetBool (Variable = Expression)", (int32)Opcode);
			AddIndent();
			output("Variable:");	SerializeExpr();
			output("Expression:");	SerializeExpr();
			DropIndent();
			break;
		}
		case EX_LetValueOnPersistentFrame: {
			output("$%X: LetValueOnPersistentFrame", (int32)Opcode);
			AddIndent();
			auto Prop = ReadPointer<FProperty>();
			output("Destination variable: %s, offset: %d", GetNameSafe(Prop), Prop ? Prop->GetOffset_ForDebug() : 0);
			output("Expression:");
			SerializeExpr();
			DropIndent();
			break;
		}
		case EX_StructMemberContext: {
			output("$%X: Struct member context ", (int32)Opcode);
			AddIndent();
			auto Prop = ReadPointer<FProperty>();
			output("Expression within struct %s, offset %d", GetName(Prop), Prop->GetOffset_ForDebug());  // although that isn't a UFunction, we are not going to indirect the props of a struct, so this should be fine
			output("Expression to struct:");	SerializeExpr();
			DropIndent();
			break;
		}
		case EX_LetDelegate: {
			output("$%X: LetDelegate (Variable = Expression)", (int32)Opcode);
			AddIndent();
			output("Variable:");	SerializeExpr();
			output("Expression:");	SerializeExpr();
			DropIndent();
			break;
		}
		case EX_LocalVirtualFunction: {
			auto FunctionName = ReadName();
			output("$%X: Local Script Function named %s", (int32)Opcode, FunctionName) override;
			while (SerializeExpr() != EX_EndFunctionParms) {}
			break;
		}
		case EX_LocalFinalFunction: {
			auto StackNode = ReadPointer<UStruct>();
			output("$%X: Local Final Script Function (stack node %s::%s)", (int32)Opcode, StackNode ? GetName(StackNode->GetOuter()) : "(null)", StackNode ? GetName(StackNode) : "(null)");
			while (SerializeExpr() != EX_EndFunctionParms) {}
			break;
		}
		case EX_LetMulticastDelegate: {
			output("$%X: LetMulticastDelegate (Variable = Expression)", (int32)Opcode);
			AddIndent();
			output("Variable:");	SerializeExpr();
			output("Expression:");	SerializeExpr();
			DropIndent();
			break;
		}
		case EX_ComputedJump: {
			output("$%X: Computed Jump, offset specified by expression:", (int32)Opcode);
			AddIndent();
			SerializeExpr();
			DropIndent();
			break;
		}
		case EX_Jump: {
			CodeSkipSizeType SkipCount = reader.get<CodeSkipSizeType>();
			output("$%X: Jump to offset 0x%X", (int32)Opcode, SkipCount);
			break;
		}
		case EX_LocalVariable: {
			auto PropertyPtr = ReadPointer<FProperty>();
			output("$%X: Local variable named %s", (int32)Opcode, PropertyPtr ? GetName(PropertyPtr) : "(null)");
			break;
		}
		case EX_DefaultVariable: {
			auto PropertyPtr = ReadPointer<FProperty>();
			output("$%X: Default variable named %s", (int32)Opcode, PropertyPtr ? GetName(PropertyPtr) : "(null)");
			break;
		}
		case EX_InstanceVariable: {
			auto PropertyPtr = ReadPointer<FProperty>();
			output("$%X: Instance variable named %s", (int32)Opcode, PropertyPtr ? GetName(PropertyPtr) : "(null)");
			break;
		}
		case EX_LocalOutVariable: {
			auto PropertyPtr = ReadPointer<FProperty>();
			output("$%X: Local out variable named %s", (int32)Opcode, PropertyPtr ? GetName(PropertyPtr) : "(null)");
			break;
		}
		case EX_ClassSparseDataVariable: {
			auto PropertyPtr = ReadPointer<FProperty>();
			output("$%X: Class sparse data variable named %s", (int32)Opcode, PropertyPtr ? GetName(PropertyPtr) : "(null)");
			break;
		}
		case EX_InterfaceContext:
			output("$%X: EX_InterfaceContext:", (int32)Opcode);
			SerializeExpr();
			break;

		case EX_DeprecatedOp4A:
			output("$%X: This opcode has been removed and does nothing.", (int32)Opcode);
			break;

		case EX_Nothing:
			output("$%X: EX_Nothing", (int32)Opcode);
			break;

		case EX_EndOfScript:
			output("$%X: EX_EndOfScript", (int32)Opcode);
			break;

		case EX_EndFunctionParms:
			output("$%X: EX_EndFunctionParms", (int32)Opcode);
			break;

		case EX_EndStructConst:
			output("$%X: EX_EndStructConst", (int32)Opcode);
			break;

		case EX_EndArray:
			output("$%X: EX_EndArray", (int32)Opcode);
			break;

		case EX_EndArrayConst:
			output("$%X: EX_EndArrayConst", (int32)Opcode);
			break;

		case EX_IntZero:
			output("$%X: EX_IntZero", (int32)Opcode);
			break;

		case EX_IntOne:
			output("$%X: EX_IntOne", (int32)Opcode);
			break;

		case EX_True:
			output("$%X: EX_True", (int32)Opcode);
			break;

		case EX_False:
			output("$%X: EX_False", (int32)Opcode);
			break;

		case EX_NoObject:
			output("$%X: EX_NoObject", (int32)Opcode);
			break;

		case EX_NoInterface:
			output("$%X: EX_NoObject", (int32)Opcode);
			break;

		case EX_Self:
			output("$%X: EX_Self", (int32)Opcode);
			break;

		case EX_EndParmValue:
			output("$%X: EX_EndParmValue", (int32)Opcode);
			break;

		case EX_Return:
			output("$%X: Return expression", (int32)Opcode);
			SerializeExpr();	 // Return expression.
			break;

		case EX_CallMath: {
			auto StackNode = ReadPointer<UStruct>();
			output("$%X: Call Math (stack node %s::%s)", (int32)Opcode, GetNameSafe(StackNode ? StackNode->GetOuter() : nullptr), GetNameSafe(StackNode));
			while (SerializeExpr() != EX_EndFunctionParms) {}
			break;
		}
		case EX_FinalFunction: {
			auto StackNode = ReadPointer<UStruct>();
			output("$%X: Final Function (stack node %s::%s)", (int32)Opcode, StackNode ? GetName(StackNode->GetOuter()) : "(null)", StackNode ? GetName(StackNode) : "(null)");
			while (SerializeExpr() != EX_EndFunctionParms) {}
			break;
		}
		case EX_CallMulticastDelegate: {
			auto StackNode = ReadPointer<UStruct>();
			output("$%X: CallMulticastDelegate (signature %s::%s) delegate:", (int32)Opcode, StackNode ? GetName(StackNode->GetOuter()) : "(null)", StackNode ? GetName(StackNode) : "(null)");
			SerializeExpr();
			output("Params:");
			while (SerializeExpr() != EX_EndFunctionParms) {}
			break;
		}
		case EX_VirtualFunction: {
			auto FunctionName = ReadName();
			output("$%X: Function named %s", (int32)Opcode, FunctionName) override;
			while (SerializeExpr() != EX_EndFunctionParms) {}
			break;
		}
		case EX_ClassContext:
		case EX_Context:
		case EX_Context_FailSilent: {
			output("$%X: %s", (int32)Opcode, Opcode == EX_ClassContext ? "Class Context" : "Context");
			AddIndent();
			output("ObjectExpression:");	SerializeExpr();

			if (Opcode == EX_Context_FailSilent)
				output(" Can fail silently on access none ");

			// Code offset for NULL expressions.
			CodeSkipSizeType SkipCount = reader.get<CodeSkipSizeType>();
			output("Skip Bytes: 0x%X", SkipCount);

			// Property corresponding to the r-value data, in case the l-value needs to be mem-zero'd
			auto Field = ReadPointer<FField>();
			output("R-Value Property: %s", Field ? GetName(Field) : "(null)");
			output("ContextExpression:");	SerializeExpr();
			DropIndent();
			break;
		}
		case EX_IntConst: {
			int32 ConstValue = reader.get<int>();
			output("$%X: literal int32 %d", (int32)Opcode, ConstValue);
			break;
		}
		case EX_SkipOffsetConst: {
			CodeSkipSizeType ConstValue = reader.get<CodeSkipSizeType>();
			output("$%X: literal CodeSkipSizeType 0x%X", (int32)Opcode, ConstValue);
			break;
		}
		case EX_FloatConst: {
			float ConstValue = reader.get<float>();
			output("$%X: literal float %f", (int32)Opcode, ConstValue);
			break;
		}
		case EX_StringConst: {
			auto ConstValue = ReadString8();
			output("$%X: literal ansi string \"%s\"", (int32)Opcode, (const char*)ConstValue);
			break;
		}
		case EX_UnicodeStringConst: {
			auto ConstValue = ReadString16();
			output("$%X: literal unicode string \"%s\"", (int32)Opcode, (const char*)ConstValue);
			break;
		}
		case EX_TextConst: {
			// What kind of text are we dealing with?
			switch (reader.get<EBlueprintTextLiteralType>()) {
				case EBlueprintTextLiteralType::Empty:
					output("$%X: literal text - empty", (int32)Opcode);
					break;

				case EBlueprintTextLiteralType::LocalizedText: {
					auto SourceString	= ReadString();
					auto KeyString		= ReadString();
					auto Namespace		= ReadString();
					output("$%X: literal text - localized text { namespace: \"%s\", key: \"%s\", source: \"%s\" }", (int32)Opcode, (const char*)Namespace, (const char*)KeyString, (const char*)SourceString);
					break;
				}
				case EBlueprintTextLiteralType::InvariantText: {
					auto SourceString	= ReadString();
					output("$%X: literal text - invariant text: \"%s\"", (int32)Opcode, (const char*)SourceString);
					break;
				}
				case EBlueprintTextLiteralType::LiteralString: {
					auto SourceString	= ReadString();
					output("$%X: literal text - literal string: \"%s\"", (int32)Opcode, (const char*)SourceString);
					break;
				}
				case EBlueprintTextLiteralType::StringTableEntry: {
					ReadPointer<UObject>();	// String Table asset (if any)
					auto TableIdString	= ReadString();
					auto KeyString		= ReadString();
					output("$%X: literal text - string table entry { tableid: \"%s\", key: \"%s\" }", (int32)Opcode, (const char*)TableIdString, (const char*)KeyString);
					break;
				}
				default:
					ISO_ASSERT(0);
			}
			break;
		}
		case EX_ObjectConst: {
			auto Pointer = ReadPointer<UObject>();
			output("$%X: EX_ObjectConst (%p:%s)", (int32)Opcode, Pointer, GetFullName(Pointer));
			break;
		}
		case EX_SoftObjectConst:
			output("$%X: EX_SoftObjectConst", (int32)Opcode);
			SerializeExpr();
			break;

		case EX_FieldPathConst:
			output("$%X: EX_FieldPathConst", (int32)Opcode);
			SerializeExpr();
			break;

		case EX_NameConst: {
			auto ConstValue = ReadName();
			output("$%X: literal name %s", (int32)Opcode, &*ConstValue);
			break;
		}
		case EX_RotationConst: {
			auto	ConstValue = reader.get<FRotator>();
			output("$%X: literal rotation (%f,%f,%f)", (int32)Opcode, ConstValue.Pitch, ConstValue.Yaw, ConstValue.Roll);
			break;
		}
		case EX_VectorConst: {
			auto	ConstValue = reader.get<FVector>();
			output("$%X: literal vector (%f,%f,%f)", (int32)Opcode, ConstValue.X, ConstValue.Y, ConstValue.Z);
			break;
		}
		case EX_TransformConst: {
			auto	ConstValue = reader.get<FTransform>();
			output("$%X: literal transform R(%f,%f,%f,%f) T(%f,%f,%f) S(%f,%f,%f)", (int32)Opcode, ConstValue.Translation.X, ConstValue.Translation.Y, ConstValue.Translation.Z, ConstValue.Rotation.X, ConstValue.Rotation.Y, ConstValue.Rotation.Z, ConstValue.Rotation.W, ConstValue.Scale3D.X, ConstValue.Scale3D.Y, ConstValue.Scale3D.Z);
			break;
		}
		case EX_StructConst: {
			auto	Struct		  = ReadPointer<UScriptStruct>();
			int32		   SerializedSize = reader.get<int>();
			output("$%X: literal struct %s (serialized size: %d)", (int32)Opcode, GetName(Struct), SerializedSize);
			while (SerializeExpr() != EX_EndStructConst) {
			}
			break;
		}
		case EX_SetArray: {
			output("$%X: set array", (int32)Opcode);
			SerializeExpr();
			while (SerializeExpr() != EX_EndArray) {
			}
			break;
		}
		case EX_ArrayConst: {
			auto InnerProp = ReadPointer<FProperty>();
			int32	   Num		 = reader.get<int>();
			output("$%X: set array const - elements number: %d, inner property: %s", (int32)Opcode, Num, GetNameSafe(InnerProp));
			while (SerializeExpr() != EX_EndArrayConst) {
			}
			break;
		}
		case EX_ByteConst: {
			uint8 ConstValue = reader.get<uint8>();
			output("$%X: literal byte %d", (int32)Opcode, ConstValue);
			break;
		}
		case EX_IntConstByte: {
			int32 ConstValue = reader.get<uint8>();
			output("$%X: literal int %d", (int32)Opcode, ConstValue);
			break;
		}
		case EX_MetaCast: {
			auto Class = ReadPointer<UClass>();
			output("$%X: MetaCast to %s of expr:", (int32)Opcode, GetName(Class));
			SerializeExpr();
			break;
		}
		case EX_DynamicCast: {
			auto Class = ReadPointer<UClass>();
			output("$%X: DynamicCast to %s of expr:", (int32)Opcode, GetName(Class));
			SerializeExpr();
			break;
		}
		case EX_JumpIfNot: {
			// Code offset.
			CodeSkipSizeType SkipCount = reader.get<CodeSkipSizeType>();
			output("$%X: Jump to offset 0x%X if not expr:", (int32)Opcode, SkipCount);
			// Boolean expr.
			SerializeExpr();
			break;
		}
		case EX_Assert: {
			uint16 LineNumber  = reader.get<uint16>();
			uint8  InDebugMode = reader.get<uint8>();
			output("$%X: assert at line %d, in debug mode = %d with expr:", (int32)Opcode, LineNumber, InDebugMode);
			SerializeExpr();	 // Assert expr.
			break;
		}
		case EX_Skip: {
			CodeSkipSizeType W = reader.get<CodeSkipSizeType>();
			output("$%X: possibly skip 0x%X bytes of expr:", (int32)Opcode, W);
			// Expression to possibly skip.
			SerializeExpr();
			break;
		}
		case EX_InstanceDelegate: {
			// the name of the function assigned to the delegate.
			auto FuncName = ReadName();
			output("$%X: instance delegate function named %s", (int32)Opcode, &*FuncName);
			break;
		}
		case EX_AddMulticastDelegate:
			output("$%X: Add MC delegate", (int32)Opcode);
			SerializeExpr();
			SerializeExpr();
			break;

		case EX_RemoveMulticastDelegate:
			output("$%X: Remove MC delegate", (int32)Opcode);
			SerializeExpr();
			SerializeExpr();
			break;

		case EX_ClearMulticastDelegate:
			output("$%X: Clear MC delegate", (int32)Opcode);
			SerializeExpr();
			break;

		case EX_BindDelegate: {
			// the name of the function assigned to the delegate.
			auto FuncName = ReadName();
			output("$%X: BindDelegate '%s' ", (int32)Opcode, &*FuncName);
			output("Delegate:");	SerializeExpr();
			output("Object:");		SerializeExpr();
			break;
		}
		case EX_PushExecutionFlow: {
			CodeSkipSizeType SkipCount = reader.get<CodeSkipSizeType>();
			output("$%X: FlowStack.Push(0x%X);", (int32)Opcode, SkipCount);
			break;
		}
		case EX_PopExecutionFlow:
			output("$%X: if (FlowStack.Num()) { jump to statement at FlowStack.Pop(); } else { ERROR!!! }", (int32)Opcode);
			break;

		case EX_PopExecutionFlowIfNot:
			output("$%X: if (!condition) { if (FlowStack.Num()) { jump to statement at FlowStack.Pop(); } else { ERROR!!! } }", (int32)Opcode);
			// Boolean expr.
			SerializeExpr();
			break;

		case EX_Breakpoint:
			output("$%X: <<< BREAKPOINT >>>", (int32)Opcode);
			break;

		case EX_WireTracepoint:
			output("$%X: .. wire debug site ..", (int32)Opcode);
			break;

		case EX_InstrumentationEvent:
			switch (reader.get<uint8>()) {
				case FScriptInstrumentationSignal::InlineEvent:			output("$%X: .. instrumented inline event ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::Stop:				output("$%X: .. instrumented event stop ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::PureNodeEntry:		output("$%X: .. instrumented pure node entry site ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::NodeDebugSite:		output("$%X: .. instrumented debug site ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::NodeEntry:			output("$%X: .. instrumented wire entry site ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::NodeExit:			output("$%X: .. instrumented wire exit site ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::PushState:			output("$%X: .. push execution state ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::RestoreState:		output("$%X: .. restore execution state ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::ResetState:			output("$%X: .. reset execution state ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::SuspendState:		output("$%X: .. suspend execution state ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::PopState:			output("$%X: .. pop execution state ..", (int32)Opcode); break;
				case FScriptInstrumentationSignal::TunnelEndOfThread:	output("$%X: .. tunnel end of thread ..", (int32)Opcode); break;
			}
			break;

		case EX_Tracepoint:
			output("$%X: .. debug site ..", (int32)Opcode);
			break;

		case EX_SwitchValue: {
			const auto NumCases	 = reader.get<uint16>();
			const auto AfterSkip = reader.get<CodeSkipSizeType>();

			output("$%X: Switch Value %d cases, end in 0x%X", (int32)Opcode, NumCases, AfterSkip);
			AddIndent();
			output("Index:");
			SerializeExpr();

			for (uint16 CaseIndex = 0; CaseIndex < NumCases; ++CaseIndex) {
				output("[%d] Case Index (label: 0x%X):", CaseIndex);
				SerializeExpr();	 // case index value term
				const auto OffsetToNextCase = reader.get<CodeSkipSizeType>();
				output("[%d] Offset to the next case: 0x%X", CaseIndex, OffsetToNextCase);
				output("[%d] Case Result:", CaseIndex);
				SerializeExpr();	 // case term
			}

			output("Default result (label: 0x%X):");
			SerializeExpr();
			output("(label: 0x%X)");
			DropIndent();
			break;
		}
		case EX_ArrayGetByRef:
			output("$%X: Array Get-by-Ref Index", (int32)Opcode);
			AddIndent();
			SerializeExpr();
			SerializeExpr();
			DropIndent();
			break;

		default:
			// This should never occur
			ISO_ASSERT(0);
			break;
	}
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

class UnrealFileHandler : public FileHandler {
	const char* GetExt() override { return "unasset"; }
	const char* GetDescription() override { return "Unreal asset"; }


	int Check(istream_ref file) override {
		file.seek(0);
		return is_any(file.get<uint32>(), FPackageFileSummary::PACKAGE_FILE_TAG,  swap_endian(FPackageFileSummary::PACKAGE_FILE_TAG)) ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void> Read(tag id, istream_ref file) override {
		FPackageFileSummary	sum;
		if (!file.read(sum))
			return ISO_NULL;

		ISO_ptr<anything>	p(id);

//		p->Append(ISO::MakePtr("summary", sum));

		//thumbnails
		ISO_ptr<anything>	thumbs("thumbnails");
		p->Append(thumbs);
		file.seek(sum.ThumbnailTableOffset);
		LoadThumbnails(file, *thumbs);

		//linker tables
		FLinkerTables	linker_tables(file, sum);

		for (auto &i : linker_tables.ExportMap)
			i.p = ISO::MakePtr<ISO_ptr<void>>(linker_tables.lookup(i.ObjectName));

		for (auto &i : linker_tables.ImportMap)
			i.p = ISO::MakePtrExternal<ISO_ptr<void>>("C:\\temp", linker_tables.lookup(i.ObjectName));

		istream_linker	file2(file, &linker_tables);
		for (auto &i : linker_tables.ExportMap) {
			ISO_TRACEF("Load ") << linker_tables.lookup(i.ObjectName) << '\n';

			if (i.SerialSize) {
				file.seek(i.SerialOffset);
				*i.p = ReadSerialised(file2, i);
				p->Append(i.p);
				if (auto remain = (i.SerialOffset + i.SerialSize) - file.tell())
					ISO_TRACEF("\tRemaining: ") << remain << '\n';
			}
		}

		// asset registry tags
		if (sum.AssetRegistryDataOffset > 0) {
			file.seek(sum.AssetRegistryDataOffset);

			ISO_ptr<anything>	reg("Registry");
			p->Append(reg);

			// Load the object count (UAsset files usually only have one asset, maps and redirectors have multiple)
			int32 n = file.get<int>();
			for (int32 i = 0; i < n; ++i) {
				FString ObjectPath		= file.get();
				FString ObjectClassName	= file.get();
				ISO_ptr<anything>	p2((const char*)ObjectPath);
				reg->Append(p2);

				p2->Append(ISO::MakePtr("Class", ObjectClassName));
				p2->Append(ISO::MakePtr("Tags", file.get<TMap<FString,FString>>()));
				int32	TagCount = file.get<int>();
				//for (int32 TagIdx = 0; TagIdx < TagCount; ++TagIdx) {
				//	FString Key		= file.get();
				//	FString Value	= file.get();
				//	if (Key && Value)
				//		p2->Append(ISO_ptr<string>((const char*)Key, (const char*)Value));
				//}
			}
		}

		return p;
	}
public:
	UnrealFileHandler() {
		ISO::getdef<UNodeMappingContainer>();
	}
} unreal;

//-----------------------------------------------------------------------------
// SaveGame
//-----------------------------------------------------------------------------

struct FSaveGameHeader {
	static const int UE4_SAVEGAME_FILE_TYPE_TAG = 0x53415647;		// "sAvG"
	enum Type {
		InitialVersion = 1,
		// serializing custom versions into the savegame data to handle that type of versioning
		AddedCustomVersions = 2,

		// -----<new versions can be added above this line>-------------------------------------------------
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
			// this is an old saved game, back up the file pointer to the beginning and assume version 1
			file.seek(0);
			SaveGameFileVersion = InitialVersion;
		} else {
			// Read version for this file format
			iso::read(file, SaveGameFileVersion, PackageFileUE4Version, SavedEngineVersion);

			if (SaveGameFileVersion >= AddedCustomVersions) {
				file.read(CustomVersionFormat);
				CustomVersions.read(file, static_cast<FCustomVersionContainer::Type>(CustomVersionFormat));
			}
		}

		// Get the class name
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
		iso::read(file, FileTypeTag, PackageFileUE4Version, SavedEngineVersion, CustomVersionFormat);
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
		istream_linker	linker(file, &tables);

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


		//return load_Class.load(linker);
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
		PakFile_Magic							= 0x5A6F12E1,
		MaxChunkDataSize						= 64*1024,
		CompressionMethodNameLen				= 32,
		MaxNumCompressionMethods				= 5,

		Version_Initial							= 1,
		Version_NoTimestamps					= 2,
		Version_CompressionEncryption			= 3,
		Version_IndexEncryption					= 4,
		Version_RelativeChunkOffsets			= 5,
		Version_DeleteRecords					= 6,
		Version_EncryptionKeyGuid				= 7,
		Version_FNameBasedCompressionMethod		= 8,
		Version_FrozenIndex						= 9,

		Version_Last,
		Version_Invalid,
		Version_Latest							= Version_Last - 1
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
		iso::read(file, bEncryptedIndex, Magic, Version, IndexOffset, IndexSize, IndexHash);
		if (Magic != PakFile_Magic)
			return false;

		if (ver < Version_IndexEncryption)
			bEncryptedIndex = false;

		if (ver < Version_EncryptionKeyGuid)
			clear(EncryptionKeyGuid);

		if (ver >= Version_FrozenIndex)
			file.read(bIndexIsFrozen);

		if (ver < Version_FNameBasedCompressionMethod) {
			// for old versions, put in some known names that we may have used
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

struct FPakCompressedBlock {
	int64	CompressedStart;
	int64	CompressedEnd;
};

enum ECompressionFlags {
	COMPRESS_None						= 0x00,
	COMPRESS_ZLIB						= 0x01,
	COMPRESS_GZIP						= 0x02,
	COMPRESS_Custom						= 0x04,
	COMPRESS_DeprecatedFormatFlagsMask	= 0x0F,
	COMPRESS_NoFlags					= 0x00,
	COMPRESS_BiasMemory					= 0x10,
	COMPRESS_BiasSpeed					= 0x20,
	COMPRESS_SourceIsPadded				= 0x80,
	COMPRESS_OptionsFlagsMask			= 0xF0,
};

struct FDateTime {
	int64 Ticks;	// Holds the ticks in 100 nanoseconds resolution since January 1, 0001 A.D
};

struct FPakEntry {
	static const uint8 Flag_None = 0x00;
	static const uint8 Flag_Encrypted = 0x01;
	static const uint8 Flag_Deleted = 0x02;
	int64		Offset;
	int64		Size;
	int64		UncompressedSize;
	uint8		Hash[20];
	TArray<FPakCompressedBlock>	CompressionBlocks;
	uint32		CompressionBlockSize;
	uint32		CompressionMethodIndex;
	uint8		Flags;
	mutable bool		Verified;

	bool	read(istream_ref file, uint32 ver) {
		file.read(Offset);
		file.read(Size);
		file.read(UncompressedSize);
		if (ver < FPakInfo::Version_FNameBasedCompressionMethod) {
			int32 LegacyCompressionMethod = file.get<int32>();
			if (LegacyCompressionMethod == COMPRESS_None)
				CompressionMethodIndex = 0;
			else if (LegacyCompressionMethod & COMPRESS_ZLIB)
				CompressionMethodIndex = 1;
			else if (LegacyCompressionMethod & COMPRESS_GZIP)
				CompressionMethodIndex = 2;
			else if (LegacyCompressionMethod & COMPRESS_Custom)
				CompressionMethodIndex = 3;
		} else {
			file.read(CompressionMethodIndex);
		}
		if (ver <= FPakInfo::Version_Initial)
			file.get<FDateTime>();

		file.read(Hash);
		if (ver >= FPakInfo::Version_CompressionEncryption) {
			if (CompressionMethodIndex)
				file.read(CompressionBlocks);
			file.read(Flags);
			file.read(CompressionBlockSize);
		}
		return true;
	}
	malloc_block	get(istream_ref file) const {
		malloc_block	data(UncompressedSize);
		if (CompressionMethodIndex == 0) {
			file.seek(Offset);
			file.readbuff(data, UncompressedSize);
		} else {
			size_t			dst	= 0;
			for (auto& i : CompressionBlocks) {
				file.seek(Offset + i.CompressedStart);
				transcode_from_file(zlib_decoder(), data + dst, file);
				dst	+= CompressionBlockSize;
			}
		}
		return data;
	}
};

typedef TMap<FString, int32> FPakDirectory;

struct FPakFileData {
	FString					MountPoint;
	TArray<FPakEntry>		Files;
	TMap<FString, FPakDirectory>	Index;
};

struct FPakFile {
	FPakInfo					info;
	unique_ptr<FPakFileData>	data;
	FString						MountPoint;
	bool	read(istream_ref file);
};

bool FPakFile::read(istream_ref file) {
	streamptr	total_size = file.length();
	for (uint32 ver			= FPakInfo::Version_Latest; ver > FPakInfo::Version_Initial; --ver) {
		auto	info_pos	= total_size - info.serialized_size(ver);
		if (info_pos >= 0) {
			file.seek(info_pos);
			if (info.read(file, ver))
				break;
		}
	}
	if (!info.valid())
		return false;

	//if (!info.EncryptionKeyGuid.IsValid() || GetRegisteredEncryptionKeys().HasKey(info.EncryptionKeyGuid))
	if (total_size < (info.IndexOffset + info.IndexSize))
		return false;

	file.seek(info.IndexOffset);

	if (info.Version >= FPakInfo::Version_FrozenIndex && info.bIndexIsFrozen) {
		// read frozen data
		data		= malloc_block(file, info.IndexSize);
		MountPoint	= move(data->MountPoint);

	} else {
#if 0
		FSHAHash EncryptedDataHash;

		// Decrypt if necessary
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

			// Construct Index of all directories in pak file.
			filename	fn		= (const char*)Filename;
			FString		path	= fn.dir().convert_to_fwdslash().begin();

			if (auto i = data->Index.find(path)) {
				FPakDirectory &Directory = *i;
				Directory.put(fn.name().begin(), EntryIndex);
			} else {
				FPakDirectory& NewDirectory = data->Index.emplace(path);
				NewDirectory.put(fn.name().begin(), EntryIndex);

				// add the parent directories up to the mount point
				for (auto &i : with_iterator(parts<'/'>((const char*)path)))
					data->Index[i.full()];
			}
		}
		ISO_TRACEF("Missing ") << total_size - entry_total - info.IndexSize << '\n';
	}
	return true;
}

ISO_DEFUSERCOMPV(FSHAHash, Hash);
ISO_DEFUSERCOMPV(FPakInfo, Magic, Version, IndexOffset, IndexSize, IndexHash, bEncryptedIndex, bIndexIsFrozen, EncryptionKeyGuid, CompressionMethods);
ISO_DEFUSERCOMPV(FPakCompressedBlock, CompressedStart, CompressedEnd);
ISO_DEFUSERCOMPV(FPakEntry, Offset, Size, UncompressedSize, Hash, CompressionBlocks, CompressionBlockSize, CompressionMethodIndex, Flags, Verified);
ISO_DEFUSERCOMPV(FPakFileData, MountPoint, Files, Index);
ISO_DEFUSERCOMPV(FPakFile, info, data, MountPoint);

struct FPakFileRoot : refs<FPakFileRoot> {
	istream_ptr			file;
	ISO_ptr<Folder>		root;
	FPakFileRoot(istream_ref file, const FPakFile& pak);
};
ISO_DEFUSERCOMPV(FPakFileRoot, root);

struct FPakFileFile : ISO::VirtualDefaults {
	ref_ptr<FPakFileRoot>	r;
	FPakEntry			entry;

	FPakFileFile(FPakFileRoot *r, const FPakEntry &entry) : r(r), entry(entry) {}

	uint32			Count() {
		return entry.UncompressedSize;
	}
	ISO_ptr<void>	Deref()	{
		return ISO::MakePtr(0, entry.get(r->file));
	}
};
ISO_DEFUSERVIRTX(FPakFileFile, "File");

FPakFileRoot::FPakFileRoot(istream_ref	file, const FPakFile& pak) : file(file.clone()), root("root") {
	for (auto& i : pak.data->Index.with_keys()) {
		ISO_TRACEF("Making folder ") << (const char*)i.k << '\n';
		auto	folder = GetDir(root, (const char*)i.k);
		for (auto j : i.v.with_keys()) {
			ISO_TRACEF("  adding file ") << (const char*)j.k << '\n';
			auto	&entry = pak.data->Files[j.v];
			folder->Append(ISO_ptr<FPakFileFile>(string((const char*)j.k) + ".uasset", this, entry));
//			folder->Append(ISO::MakePtr(string((const char*)j.k) + ".uasset", entry));
		}
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