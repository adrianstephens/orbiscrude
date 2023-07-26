#include "iso/iso_files.h"
#include "base/list.h"
#include "extra/text_stream.h"

using namespace iso;

namespace nif {
const uint32 VER_INVALID		= 0; // Not a NIF file
const uint32 VER_2_3			= 0x02030000; // NIF Version 2.3
const uint32 VER_3_0			= 0x03000000; // NIF Version 3.0
const uint32 VER_3_03			= 0x03000300; // NIF Version 3.03
const uint32 VER_3_1			= 0x03010000; // NIF Version 3.1
const uint32 VER_3_3_0_13		= 0x0303000D; // NIF Version 3.3.0.13
const uint32 VER_4_0_0_0		= 0x04000000; // NIF Version 4.0.0.0
const uint32 VER_4_0_0_2		= 0x04000002; // NIF Version 4.0.0.2
const uint32 VER_4_1_0_12		= 0x0401000C; // NIF Version 4.1.0.12
const uint32 VER_4_2_0_2		= 0x04020002; // NIF Version 4.2.0.2
const uint32 VER_4_2_1_0		= 0x04020100; // NIF Version 4.2.1.0
const uint32 VER_4_2_2_0		= 0x04020200; // NIF Version 4.2.2.0
const uint32 VER_10_0_1_0		= 0x0A000100; // NIF Version 10.0.1.0
const uint32 VER_10_0_1_2		= 0x0A000102; // NIF Version 10.0.1.2
const uint32 VER_10_0_1_3		= 0x0A000103; // NIF Version 10.0.1.3
const uint32 VER_10_1_0_0		= 0x0A010000; // NIF Version 10.1.0.0
const uint32 VER_10_1_0_101		= 0x0A010065; // NIF Version 10.1.0.101
const uint32 VER_10_1_0_106		= 0x0A01006A; // NIF Version 10.1.0.106
const uint32 VER_10_2_0_0		= 0x0A020000; // NIF Version 10.2.0.0
const uint32 VER_10_4_0_1		= 0x0A040001; // NIF Version 10.4.0.1
const uint32 VER_20_0_0_4		= 0x14000004; // NIF Version 20.0.0.4
const uint32 VER_20_0_0_5		= 0x14000005; // NIF Version 20.0.0.4
const uint32 VER_20_1_0_3		= 0x14010003; // NIF Version 20.1.0.3
const uint32 VER_20_2_0_7		= 0x14020007; // NIF Version 20.2.0.7
const uint32 VER_20_2_0_8		= 0x14020008; // NIF Version 20.2.0.7
const uint32 VER_20_3_0_1		= 0x14030001; // NIF Version 20.3.0.1
const uint32 VER_20_3_0_2		= 0x14030002; // NIF Version 20.3.0.2
const uint32 VER_20_3_0_3		= 0x14030003; // NIF Version 20.3.0.3
const uint32 VER_20_3_0_6		= 0x14030006; // NIF Version 20.3.0.6
const uint32 VER_20_3_0_9		= 0x14030009; // NIF Version 20.3.0.9
const uint32 VER_UNSUPPORTED	= 0xFFFFFFFF; // Unsupported NIF Version

// The type of force?  May be more valid values.
enum ForceType {
	FORCE_PLANAR				= 0, // FORCE_PLANAR
	FORCE_SPHERICAL				= 1, // FORCE_SPHERICAL
	FORCE_UNKNOWN				= 2, // FORCE_UNKNOWN
};

enum CollisionMode {
	CM_USE_OBB					= 0, // Use Bounding Box
	CM_USE_TRI					= 1, // Use Triangles
	CM_USE_ABV					= 2, // Use Alternate Bounding Volumes
	CM_NOTEST					= 3, // No Test
	CM_USE_NIBOUND				= 4, // Use NiBound
};

enum DeactivatorType {
	DEACTIVATOR_INVALID			= 0, // Invalid
	DEACTIVATOR_NEVER			= 1, // This will force the rigid body to never deactivate.
	DEACTIVATOR_SPATIAL			= 2, // Tells Havok to use a spatial deactivation scheme. This makes use of high and low frequencies of positional motion to determine when deactivation should occur.
};

enum hkResponseType {
	RESPONSE_INVALID			= 0, // Invalid Response
	RESPONSE_SIMPLE_CONTACT		= 1, // Do normal collision resolution
	RESPONSE_REPORTING			= 2, // No collision resolution is performed but listeners are called
	RESPONSE_NONE				= 3, // Do nothing, ignore all the results.
};

// Determines how a NiTextureTransformController animates the UV coordinates.
enum TexTransform {
	TT_TRANSLATE_U				= 0, //!< Means this controller moves the U texture cooridnates.
	TT_TRANSLATE_V				= 1, //< Means this controller moves the V texture cooridnates.
	TT_ROTATE					= 2, //< Means this controller roates the UV texture cooridnates.
	TT_SCALE_U					= 3, // Means this controller scales the U texture cooridnates.
	TT_SCALE_V					= 4, // Means this controller scales the V texture cooridnates.
};

// A material, used by havok shape objects
enum HavokMaterial {
	HAV_MAT_STONE				= 0, // Stone
	HAV_MAT_CLOTH				= 1, // Cloth
	HAV_MAT_DIRT				= 2, // Dirt
	HAV_MAT_GLASS				= 3, // Glass
	HAV_MAT_GRASS				= 4, // Grass
	HAV_MAT_METAL				= 5, // Metal
	HAV_MAT_ORGANIC				= 6, // Organic
	HAV_MAT_SKIN				= 7, // Skin
	HAV_MAT_WATER				= 8, // Water
	HAV_MAT_WOOD				= 9, // Wood
	HAV_MAT_HEAVY_STONE			= 10, // Heavy Stone
	HAV_MAT_HEAVY_METAL			= 11, // Heavy Metal
	HAV_MAT_HEAVY_WOOD			= 12, // Heavy Wood
	HAV_MAT_CHAIN				= 13, // Chain
	HAV_MAT_SNOW				= 14, // Snow
	HAV_MAT_STONE_STAIRS		= 15, // Stone Stairs
	HAV_MAT_CLOTH_STAIRS		= 16, // Cloth Stairs
	HAV_MAT_DIRT_STAIRS			= 17, // Dirt Stairs
	HAV_MAT_GLASS_STAIRS		= 18, // Glass Stairs
	HAV_MAT_GRASS_STAIRS		= 19, // Grass Stairs
	HAV_MAT_METAL_STAIRS		= 20, // Metal Stairs
	HAV_MAT_ORGANIC_STAIRS		= 21, // Organic Stairs
	HAV_MAT_SKIN_STAIRS			= 22, // Skin Stairs
	HAV_MAT_WATER_STAIRS		= 23, // Water Stairs
	HAV_MAT_WOOD_STAIRS			= 24, // Wood Stairs
	HAV_MAT_HEAVY_STONE_STAIRS	= 25, // Heavy Stone Stairs
	HAV_MAT_HEAVY_METAL_STAIRS	= 26, // Heavy Metal Stairs
	HAV_MAT_HEAVY_WOOD_STAIRS	= 27, // Heavy Wood Stairs
	HAV_MAT_CHAIN_STAIRS		= 28, // Chain Stairs
	HAV_MAT_SNOW_STAIRS			= 29, // Snow Stairs
	HAV_MAT_ELEVATOR			= 30, // Elevator
	HAV_MAT_RUBBER				= 31, // Rubber
};

enum EndianType {
	ENDIAN_BIG					= 0, // The numbers are stored in big endian format, such as those used by PowerPC Mac processors.
	ENDIAN_LITTLE				= 1, // The numbers are stored in little endian format, such as those used by Intel and AMD x86 processors.
};

// Controls the way the a particle mesh emitter determines the starting speed and direction of the particles that are emitted.
enum VelocityType {
	VELOCITY_USE_NORMALS		= 0, // Uses the normals of the meshes to determine staring velocity.
	VELOCITY_USE_RANDOM			= 1, // Starts particles with a random velocity.
	VELOCITY_USE_DIRECTION		= 2, // Uses the emission axis to determine initial particle direction?
};

// Animation type used on this position. This specifies the function of this position.
enum AnimationType {
	SIT							= 1, // Actor use sit animation.
	SLEEP						= 2, // Actor use sleep animation.
	LEAN						= 4, // Used for lean animations?
};

// Determines how a data stream is used?
enum DataStreamUsage {
	USAGE_VERTEX_INDEX			= 0, // USAGE_VERTEX_INDEX
	USAGE_VERTEX				= 1, // USAGE_VERTEX
	USAGE_SHADER_CONSTANT		= 2, // USAGE_SHADER_CONSTANT
	USAGE_USER					= 3, // USAGE_USER
};

// This enum contains the options for doing stencil buffer tests.
enum StencilCompareMode {
	TEST_NEVER					= 0, // Test will allways return false. Nothing is drawn at all.
	TEST_LESS					= 1, // The test will only succeed if the pixel is nearer than the previous pixel.
	TEST_EQUAL					= 2, // Test will only succeed if the z value of the pixel to be drawn is equal to the value of the previous drawn pixel.
	TEST_LESS_EQUAL				= 3, // Test will succeed if the z value of the pixel to be drawn is smaller than or equal to the value in the Stencil Buffer.
	TEST_GREATER				= 4, // Opposite of TEST_LESS.
	TEST_NOT_EQUAL				= 5, // Test will succeed if the z value of the pixel to be drawn is NOT equal to the value of the previously drawn pixel.
	TEST_GREATER_EQUAL			= 6, // Opposite of TEST_LESS_EQUAL.
	TEST_ALWAYS					= 7, // Test will allways succeed. The Stencil Buffer value is ignored.
};


// Specifies the availiable texture filter modes.  That is, the way pixels within a texture are blended together when textures are displayed on the screen at a size other than their original dimentions.
enum TexFilterMode {
	FILTER_NEAREST				= 0, // Simply uses the nearest pixel.  Very grainy.
	FILTER_BILERP				= 1, // Uses bilinear filtering.
	FILTER_TRILERP				= 2, // Uses trilinear filtering.
	FILTER_NEAREST_MIPNEAREST	= 3, // Uses the nearest pixel from the mipmap that is closest to the display size.
	FILTER_NEAREST_MIPLERP		= 4, // Blends the two mipmaps closest to the display size linearly, and then uses the nearest pixel from the result.
	FILTER_BILERP_MIPNEAREST	= 5, // Uses the closest mipmap to the display size and then uses bilinear filtering on the pixels.
};

// Determines symetry type used by NiPSysBombModifier.
enum SymmetryType {
	SPHERICAL_SYMMETRY			= 0, // Spherical Symmetry.
	CYLINDRICAL_SYMMETRY		= 1, // Cylindrical Symmetry.
	PLANAR_SYMMETRY				= 2, // Planar Symmetry.
};

// An unsigned 32-bit integer, describing how vertex colors influence lighting.
enum LightMode {
	LIGHT_MODE_EMISSIVE			= 0, // Emissive.
	LIGHT_MODE_EMI_AMB_DIF		= 1, // Emissive + Ambient + Diffuse. (Default)
};

// An unsigned 32-bit integer, describing how mipmaps are handled in a texture.
enum MipMapFormat {
	MIP_FMT_NO					= 0, // Texture does not use mip maps.
	MIP_FMT_YES					= 1, // Texture uses mip maps.
	MIP_FMT_DEFAULT				= 2, // Use default setting.
};

// This enum defines the various actions used in conjunction with the stencil buffer.
enum StencilAction {
	ACTION_KEEP					= 0, // ACTION_KEEP
	ACTION_ZERO					= 1, // ACTION_ZERO
	ACTION_REPLACE				= 2, // ACTION_REPLACE
	ACTION_INCREMENT			= 3, // ACTION_INCREMENT
	ACTION_DECREMENT			= 4, // ACTION_DECREMENT
	ACTION_INVERT				= 5, // ACTION_INVERT
};

// Specifies the time when an application must syncronize for some reason.
enum SyncPoint {
	SYNC_ANY					= 0x8000, // Value used when no specific sync point is desired.
	SYNC_UPDATE					= 0x8010, // Synchronize when an object is updated.
	SYNC_POST_UPDATE			= 0x8020, // Synchronize when an entire scene graph has been updated.
	SYNC_VISIBLE				= 0x8030, // Synchronize when an object is determined to be potentially visible.
	SYNC_RENDER					= 0x8040, // Synchronize when an object is rendered.
	SYNC_PHYSICS_SIMULATE		= 0x8050, // Synchronize when a physics simulation step is about to begin.
	SYNC_PHYSICS_COMPLETED		= 0x8060, // Synchronize when a physics simulation step has produced results.
	SYNC_REFLECTIONS			= 0x8070, // Syncronize after all data necessary to calculate reflections is ready.
};

// The type of texture.
enum TexType {
	BASE_MAP					= 0, // The basic texture used by most meshes.
	DARK_MAP					= 1, // Used to darken the model with false lighting.
	DETAIL_MAP					= 2, // Combined with base map for added detail.  Usually tiled over the mesh many times for close-up view.
	GLOSS_MAP					= 3, // Allows the specularity (glossyness) of an object to differ across its surface.
	GLOW_MAP					= 4, // Creates a glowing effect.  Basically an incandescence map.
	BUMP_MAP					= 5, // Used to make the object appear to have more detail than it really does.
	NORMAL_MAP					= 6, // Used to make the object appear to have more detail than it really does.
	UNKNOWN2_MAP				= 7, // Unknown map.
	DECAL_0_MAP					= 8, // For placing images on the object like stickers.
	DECAL_1_MAP					= 9, // For placing images on the object like stickers.
	DECAL_2_MAP					= 10, // For placing images on the object like stickers.
	DECAL_3_MAP					= 11, // For placing images on the object like stickers.
};

// Biped bodypart data used for visibility control of triangles
enum BSDismemberBodyPartType {
	BP_TORSO										= 0, // Torso
	BP_HEAD											= 1, // Head
	BP_HEAD2										= 2, // Head 2
	BP_LEFTARM										= 3, // Left Arm
	BP_LEFTARM2										= 4, // Left Arm 2
	BP_RIGHTARM										= 5, // Right Arm
	BP_RIGHTARM2									= 6, // Right Arm 2
	BP_LEFTLEG										= 7, // Left Leg
	BP_LEFTLEG2										= 8, // Left Leg 2
	BP_LEFTLEG3										= 9, // Left Leg 3
	BP_RIGHTLEG										= 10, // Right Leg
	BP_RIGHTLEG2									= 11, // Right Leg 2
	BP_RIGHTLEG3									= 12, // Right Leg 3
	BP_BRAIN										= 13, // Brain
	SBP_30_HEAD										= 30, // Skyrim, Head(Human), Body(Atronachs,Beasts), Mask(Dragonpriest)
	SBP_31_HAIR										= 31, // Skyrim, Hair(human), Far(Dragon), Mask2(Dragonpriest),SkinnedFX(Spriggan)
	SBP_32_BODY										= 32, // Skyrim, Main body, extras(Spriggan)
	SBP_33_HANDS									= 33, // Skyrim, Hands L/R, BodyToo(Dragonpriest), Legs(Draugr), Arms(Giant)
	SBP_34_FOREARMS									= 34, // Skyrim, Forearms L/R, Beard(Draugr)
	SBP_35_AMULET									= 35, // Skyrim, Amulet
	SBP_36_RING										= 36, // Skyrim, Ring
	SBP_37_FEET										= 37, // Skyrim, Feet L/R
	SBP_38_CALVES									= 38, // Skyrim, Calves L/R
	SBP_39_SHIELD									= 39, // Skyrim, Shield
	SBP_40_TAIL										= 40, // Skyrim, Tail(Argonian/Khajiit), Skeleton01(Dragon), FX01(AtronachStorm),FXMist (Dragonpriest), Spit(Chaurus,Spider),SmokeFins(IceWraith)
	SBP_41_LONGHAIR									= 41, // Skyrim, Long Hair(Human), Skeleton02(Dragon),FXParticles(Dragonpriest)
	SBP_42_CIRCLET									= 42, // Skyrim, Circlet(Human, MouthFireEffect(Dragon)
	SBP_43_EARS										= 43, // Skyrim, Ears
	SBP_44_DRAGON_BLOODHEAD_OR_MOD_MOUTH			= 44, // Skyrim, Bloodied dragon head, or NPC face/mouth
	SBP_45_DRAGON_BLOODWINGL_OR_MOD_NECK			= 45, // Skyrim, Left Bloodied dragon wing, Saddle(Horse), or NPC cape, scarf, shawl, neck-tie, etc.
	SBP_46_DRAGON_BLOODWINGR_OR_MOD_CHEST_PRIMARY	= 46, // Skyrim, Right Bloodied dragon wing, or NPC chest primary or outergarment
	SBP_47_DRAGON_BLOODTAIL_OR_MOD_BACK				= 47, // Skyrim, Bloodied dragon tail, or NPC backpack/wings/...
	SBP_48_MOD_MISC1								= 48, // Anything that does not fit in the list
	SBP_49_MOD_PELVIS_PRIMARY						= 49, // Pelvis primary or outergarment
	SBP_50_DECAPITATEDHEAD							= 50, // Skyrim, Decapitated Head
	SBP_51_DECAPITATE								= 51, // Skyrim, Decapitate, neck gore
	SBP_52_MOD_PELVIS_SECONDARY						= 52, // Pelvis secondary or undergarment
	SBP_53_MOD_LEG_RIGHT							= 53, // Leg primary or outergarment or right leg
	SBP_54_MOD_LEG_LEFT								= 54, // Leg secondary or undergarment or left leg
	SBP_55_MOD_FACE_JEWELRY							= 55, // Face alternate or jewelry
	SBP_56_MOD_CHEST_SECONDARY						= 56, // Chest secondary or undergarment
	SBP_57_MOD_SHOULDER								= 57, // Shoulder
	SBP_58_MOD_ARM_LEFT								= 58, // Arm secondary or undergarment or left arm
	SBP_59_MOD_ARM_RIGHT							= 59, // Arm primary or outergarment or right arm
	SBP_60_MOD_MISC2								= 60, // Anything that does not fit in the list
	SBP_61_FX01										= 61, // Skyrim, FX01(Humanoid)
	BP_SECTIONCAP_HEAD								= 101, // Section Cap | Head
	BP_SECTIONCAP_HEAD2								= 102, // Section Cap | Head 2
	BP_SECTIONCAP_LEFTARM							= 103, // Section Cap | Left Arm
	BP_SECTIONCAP_LEFTARM2							= 104, // Section Cap | Left Arm 2
	BP_SECTIONCAP_RIGHTARM							= 105, // Section Cap | Right Arm
	BP_SECTIONCAP_RIGHTARM2							= 106, // Section Cap | Right Arm 2
	BP_SECTIONCAP_LEFTLEG							= 107, // Section Cap | Left Leg
	BP_SECTIONCAP_LEFTLEG2							= 108, // Section Cap | Left Leg 2
	BP_SECTIONCAP_LEFTLEG3							= 109, // Section Cap | Left Leg 3
	BP_SECTIONCAP_RIGHTLEG							= 110, // Section Cap | Right Leg
	BP_SECTIONCAP_RIGHTLEG2							= 111, // Section Cap | Right Leg 2
	BP_SECTIONCAP_RIGHTLEG3							= 112, // Section Cap | Right Leg 3
	BP_SECTIONCAP_BRAIN								= 113, // Section Cap | Brain
	SBP_130_HEAD									= 130, // Skyrim, Head slot, use on full-face helmets
	SBP_131_HAIR									= 131, // Skyrim, Hair slot 1, use on hoods
	SBP_141_LONGHAIR								= 141, // Skyrim, Hair slot 2, use for longer hair
	SBP_142_CIRCLET									= 142, // Skyrim, Circlet slot 1, use for circlets
	SBP_143_EARS									= 143, // Skyrim, Ear slot
	SBP_150_DECAPITATEDHEAD							= 150, // Skyrim, neck gore on head side
	BP_TORSOCAP_HEAD								= 201, // Torso Cap | Head
	BP_TORSOCAP_HEAD2								= 202, // Torso Cap | Head 2
	BP_TORSOCAP_LEFTARM								= 203, // Torso Cap | Left Arm
	BP_TORSOCAP_LEFTARM2							= 204, // Torso Cap | Left Arm 2
	BP_TORSOCAP_RIGHTARM							= 205, // Torso Cap | Right Arm
	BP_TORSOCAP_RIGHTARM2							= 206, // Torso Cap | Right Arm 2
	BP_TORSOCAP_LEFTLEG								= 207, // Torso Cap | Left Leg
	BP_TORSOCAP_LEFTLEG2							= 208, // Torso Cap | Left Leg 2
	BP_TORSOCAP_LEFTLEG3							= 209, // Torso Cap | Left Leg 3
	BP_TORSOCAP_RIGHTLEG							= 210, // Torso Cap | Right Leg
	BP_TORSOCAP_RIGHTLEG2							= 211, // Torso Cap | Right Leg 2
	BP_TORSOCAP_RIGHTLEG3							= 212, // Torso Cap | Right Leg 3
	BP_TORSOCAP_BRAIN								= 213, // Torso Cap | Brain
	SBP_230_HEAD									= 230, // Skyrim, Head slot, use for neck on character head
	BP_TORSOSECTION_HEAD							= 1000, // Torso Section | Head
	BP_TORSOSECTION_HEAD2							= 2000, // Torso Section | Head 2
	BP_TORSOSECTION_LEFTARM							= 3000, // Torso Section | Left Arm
	BP_TORSOSECTION_LEFTARM2						= 4000, // Torso Section | Left Arm 2
	BP_TORSOSECTION_RIGHTARM						= 5000, // Torso Section | Right Arm
	BP_TORSOSECTION_RIGHTARM2						= 6000, // Torso Section | Right Arm 2
	BP_TORSOSECTION_LEFTLEG							= 7000, // Torso Section | Left Leg
	BP_TORSOSECTION_LEFTLEG2						= 8000, // Torso Section | Left Leg 2
	BP_TORSOSECTION_LEFTLEG3						= 9000, // Torso Section | Left Leg 3
	BP_TORSOSECTION_RIGHTLEG						= 10000, // Torso Section | Right Leg
	BP_TORSOSECTION_RIGHTLEG2						= 11000, // Torso Section | Right Leg 2
	BP_TORSOSECTION_RIGHTLEG3						= 12000, // Torso Section | Right Leg 3
	BP_TORSOSECTION_BRAIN							= 13000, // Torso Section | Brain
};

enum ChannelType {
	CHNL_RED					= 0, // Red
	CHNL_GREEN					= 1, // Green
	CHNL_BLUE					= 2, // Blue
	CHNL_ALPHA					= 3, // Alpha
	CHNL_COMPRESSED				= 4, // Compressed
	CHNL_INDEX					= 16, // Index
	CHNL_EMPTY					= 19, // Empty
};

// Skyrim, sets what sky function this object fulfills in BSSkyShaderProperty.
enum SkyObjectType {
	BSSM_SKY_TEXTURE			= 0, // BSSM_Sky_Texture
	BSSM_SKY_SUNGLARE			= 1, // BSSM_Sky_Sunglare
	BSSM_SKY					= 2, // BSSM_Sky
	BSSM_SKY_CLOUDS				= 3, // BSSM_Sky_Clouds
	BSSM_SKY_STARS				= 5, // BSSM_Sky_Stars
	BSSM_SKY_MOON_STARS_MASK	= 7, // BSSM_Sky_Moon_Stars_Mask
};

/*!
 * Sets mesh color in Oblivion Construction Set.  Anything higher than 57 is also
 * null.
 */
enum OblivionLayer {
	OL_UNIDENTIFIED				= 0, // Unidentified (white)
	OL_STATIC					= 1, // Static (red)
	OL_ANIM_STATIC				= 2, // AnimStatic (magenta)
	OL_TRANSPARENT				= 3, // Transparent (light pink)
	OL_CLUTTER					= 4, // Clutter (light blue)
	OL_WEAPON					= 5, // Weapon (orange)
	OL_PROJECTILE				= 6, // Projectile (light orange)
	OL_SPELL					= 7, // Spell (cyan)
	OL_BIPED					= 8, // Biped (green) Seems to apply to all creatures/NPCs
	OL_TREES					= 9, // Trees (light brown)
	OL_PROPS					= 10, // Props (magenta)
	OL_WATER					= 11, // Water (cyan)
	OL_TRIGGER					= 12, // Trigger (light grey)
	OL_TERRAIN					= 13, // Terrain (light yellow)
	OL_TRAP						= 14, // Trap (light grey)
	OL_NONCOLLIDABLE			= 15, // NonCollidable (white)
	OL_CLOUD_TRAP				= 16, // CloudTrap (greenish grey)
	OL_GROUND					= 17, // Ground (none)
	OL_PORTAL					= 18, // Portal (green)
	OL_STAIRS					= 19, // Stairs (white)
	OL_CHAR_CONTROLLER			= 20, // CharController (yellow)
	OL_AVOID_BOX				= 21, // AvoidBox (dark yellow)
	OL_UNKNOWN1					= 22, // ? (white)
	OL_UNKNOWN2					= 23, // ? (white)
	OL_CAMERA_PICK				= 24, // CameraPick (white)
	OL_ITEM_PICK				= 25, // ItemPick (white)
	OL_LINE_OF_SIGHT			= 26, // LineOfSight (white)
	OL_PATH_PICK				= 27, // PathPick (white)
	OL_CUSTOM_PICK_1			= 28, // CustomPick1 (white)
	OL_CUSTOM_PICK_2			= 29, // CustomPick2 (white)
	OL_SPELL_EXPLOSION			= 30, // SpellExplosion (white)
	OL_DROPPING_PICK			= 31, // DroppingPick (white)
	OL_OTHER					= 32, // Other (white)
	OL_HEAD						= 33, // Head
	OL_BODY						= 34, // Body
	OL_SPINE1					= 35, // Spine1
	OL_SPINE2					= 36, // Spine2
	OL_L_UPPER_ARM				= 37, // LUpperArm
	OL_L_FOREARM				= 38, // LForeArm
	OL_L_HAND					= 39, // LHand
	OL_L_THIGH					= 40, // LThigh
	OL_L_CALF					= 41, // LCalf
	OL_L_FOOT					= 42, // LFoot
	OL_R_UPPER_ARM				= 43, // RUpperArm
	OL_R_FOREARM				= 44, // RForeArm
	OL_R_HAND					= 45, // RHand
	OL_R_THIGH					= 46, // RThigh
	OL_R_CALF					= 47, // RCalf
	OL_R_FOOT					= 48, // RFoot
	OL_TAIL						= 49, // Tail
	OL_SIDE_WEAPON				= 50, // SideWeapon
	OL_SHIELD					= 51, // Shield
	OL_QUIVER					= 52, // Quiver
	OL_BACK_WEAPON				= 53, // BackWeapon
	OL_BACK_WEAPON2				= 54, // BackWeapon (?)
	OL_PONYTAIL					= 55, // PonyTail
	OL_WING						= 56, // Wing
	OL_NULL						= 57, // Null
};

/*!
 * Physical purpose of collision object? The setting affects objetct's
 * havok behavior in game. Anything higher than 47 is also null.
 */
enum SkyrimLayer {
	SKYL_UNIDENTIFIED			= 0, // Unidentified
	SKYL_STATIC					= 1, // Static
	SKYL_ANIMSTATIC				= 2, // Anim Static
	SKYL_TRANSPARENT			= 3, // Transparent
	SKYL_CLUTTER				= 4, // Clutter. Object with this layer will float on water surface.
	SKYL_WEAPON					= 5, // Weapon
	SKYL_PROJECTILE				= 6, // Projectile
	SKYL_SPELL					= 7, // Spell
	SKYL_BIPED					= 8, // Biped. Seems to apply to all creatures/NPCs
	SKYL_TREES					= 9, // Trees
	SKYL_PROPS					= 10, // Props
	SKYL_WATER					= 11, // Water
	SKYL_TRIGGER				= 12, // Trigger
	SKYL_TERRAIN				= 13, // Terrain
	SKYL_TRAP					= 14, // Trap
	SKYL_NONCOLLIDABLE			= 15, // NonCollidable
	SKYL_CLOUD_TRAP				= 16, // CloudTrap
	SKYL_GROUND					= 17, // Ground. It seems that produces no sound when collide.
	SKYL_PORTAL					= 18, // Portal
	SKYL_DEBRIS_SMALL			= 19, // Debris Small
	SKYL_DEBRIS_LARGE			= 20, // Debris Large
	SKYL_ACOUSTIC_SPACE			= 21, // Acoustic Space
	SKYL_ACTORZONE				= 22, // Actor Zone
	SKYL_PROJECTILEZONE			= 23, // Projectile Zone
	SKYL_GASTRAP				= 24, // Gas Trap
	SKYL_SHELLCASING			= 25, // Shell Casing
	SKYL_TRANSPARENT_SMALL		= 26, // Transparent Small
	SKYL_INVISIBLE_WALL			= 27, // Invisible Wall
	SKYL_TRANSPARENT_SMALL_ANIM	= 28, // Transparent Small Anim
	SKYL_WARD					= 29, // Ward
	SKYL_CHARCONTROLLER			= 30, // Char Controller
	SKYL_STAIRHELPER			= 31, // Stair Helper
	SKYL_DEADBIP				= 32, // Dead Bip
	SKYL_BIPED_NO_CC			= 33, // Biped No CC
	SKYL_AVOIDBOX				= 34, // Avoid Box
	SKYL_COLLISIONBOX			= 35, // Collision Box
	SKYL_CAMERASHPERE			= 36, // Camera Sphere
	SKYL_DOORDETECTION			= 37, // Door Detection
	SKYL_CONEPROJECTILE			= 38, // Cone Projectile
	SKYL_CAMERAPICK				= 39, // Camera Pick
	SKYL_ITEMPICK				= 40, // Item Pick
	SKYL_LINEOFSIGHT			= 41, // Line of Sight
	SKYL_PATHPICK				= 42, // Path Pick
	SKYL_CUSTOMPICK1			= 43, // Custom Pick 1
	SKYL_CUSTOMPICK2			= 44, // Custom Pick 2
	SKYL_SPELLEXPLOSION			= 45, // Spell Explosion
	SKYL_DROPPINGPICK			= 46, // Dropping Pick
	SKYL_NULL					= 47 // Null
};

enum PSLoopBehavior {
	PS_LOOP_CLAMP_BIRTH			= 0, // Key times map such that the first key occurs at the birth of the particle, and times later than the last key get the last key value.
	PS_LOOP_CLAMP_DEATH			= 1, // Key times map such that the last key occurs at the death of the particle, and times before the initial key time get the value of the initial key.
	PS_LOOP_AGESCALE			= 2, // Scale the animation to fit the particle lifetime, so that the first key is age zero, and the last key comes at the particle death.
	PS_LOOP_LOOP				= 3, // The time is converted to one within the time range represented by the keys, as if the key sequence loops forever in the past and future.
	PS_LOOP_REFLECT				= 4, // The time is reflection looped, as if the keys played forward then backward the forward then backward etc for all time.
};

// A list of possible solver deactivation settings. This value defines how the solver deactivates objects. The solver works on a per object basis.
enum SolverDeactivation {
	SOLVER_DEACTIVATION_INVALID	= 0, // Invalid
	SOLVER_DEACTIVATION_OFF		= 1, // No solver deactivation
	SOLVER_DEACTIVATION_LOW		= 2, // Very conservative deactivation, typically no visible artifacts.
	SOLVER_DEACTIVATION_MEDIUM	= 3, // Normal deactivation, no serious visible artifacts in most cases
	SOLVER_DEACTIVATION_HIGH	= 4, // Fast deactivation, visible artifacts
	SOLVER_DEACTIVATION_MAX		= 5, // Very fast deactivation, visible artifacts
};

// Describes the type of primitives stored in a mesh object.
enum MeshPrimitiveType {
	MESH_PRIMITIVE_TRIANGLES	= 0, // Triangle primitive type.
	MESH_PRIMITIVE_TRISTRIPS	= 1, // Triangle strip primitive type.
	MESH_PRIMITIVE_LINESTRIPS	= 2, // Line strip primitive type.
	MESH_PRIMITIVE_QUADS		= 3, // Quadrilateral primitive type.
	MESH_PRIMITIVE_POINTS		= 4, // Point primitive type.
};

// This enum lists the different face culling options.
enum FaceDrawMode {
	DRAW_CCW_OR_BOTH			= 0, // use application defaults?
	DRAW_CCW					= 1, // Draw counter clock wise faces, cull clock wise faces. This is the default for most (all?) Nif Games so far.
	DRAW_CW						= 2, // Draw clock wise faces, cull counter clock wise faces. This will flip all the faces.
	DRAW_BOTH					= 3, // Draw double sided faces.
};

// An unsigned 32-bit integer, describing how transparency is handled in a texture.
enum AlphaFormat {
	ALPHA_NONE					= 0, // No alpha blending; the texture is fully opaque.
	ALPHA_BINARY				= 1, // Texture is either fully transparent or fully opaque.  There are no partially transparent areas.
	ALPHA_SMOOTH				= 2, // Full range of alpha values can be used from fully transparent to fully opaque including all partially transparent values in between.
	ALPHA_DEFAULT				= 3, // Use default setting.
};

enum BoundVolumeType {
	BASE_BV						= 0xffffffff, // Default
	SPHERE_BV					= 0, // Sphere
	BOX_BV						= 1, // Box
	CAPSULE_BV					= 2, // Capsule
	UNION_BV					= 4, // Union
	HALFSPACE_BV				= 5, // Half Space
};

// The type of information that's store in a texture used by a NiTextureEffect.
enum EffectType {
	EFFECT_PROJECTED_LIGHT		= 0, // Apply a projected light texture.
	EFFECT_PROJECTED_SHADOW		= 1, // Apply a projected shaddow texture.
	EFFECT_ENVIRONMENT_MAP		= 2, // Apply an environment map texture.
	EFFECT_FOG_MAP				= 3, // Apply a fog map texture.
};

// The type of animation interpolation (blending) that will be used on the associated key frames.
enum KeyType {
	LINEAR_KEY					= 1, // Use linear interpolation.
	QUADRATIC_KEY				= 2, // Use quadratic interpolation.  Forward and back tangents will be stored.
	TBC_KEY						= 3, // Use Tension Bias Continuity interpolation.  Tension, bias, and continuity will be stored.
	XYZ_ROTATION_KEY			= 4, // For use only with rotation data.  Separate X, Y, and Z keys will be stored instead of using quaternions.
	CONST_KEY					= 5, // Step function. Used for visibility keys in NiBoolData.
};

// An unsigned 32-bit integer, which describes how to apply vertex colors.
enum VertMode {
	VERT_MODE_SRC_IGNORE		= 0, // Source Ignore.
	VERT_MODE_SRC_EMISSIVE		= 1, // Source Emissive.
	VERT_MODE_SRC_AMB_DIF		= 2, // Source Ambient/Diffuse. (Default)
};

enum SortingMode {
	SORTING_INHERIT				= 0, // Inherit
	SORTING_OFF					= 1, // Disable
};

// Determines the way that UV texture coordinates are generated.
enum CoordGenType {
	CG_WORLD_PARALLEL			= 0, // Use plannar mapping.
	CG_WORLD_PERSPECTIVE		= 1, // Use perspective mapping.
	CG_SPHERE_MAP				= 2, // Use spherical mapping.
	CG_SPECULAR_CUBE_MAP		= 3, // Use specular cube mapping.
	CG_DIFFUSE_CUBE_MAP			= 4, // Use Diffuse cube mapping.
};

// The type of animation interpolation (blending) that will be used on the associated key frames.
enum BSShaderType {
	SHADER_TALL_GRASS			= 0, // Tall Grass Shader
	SHADER_DEFAULT				= 1, // Standard Lighting Shader
	SHADER_SKY					= 10, // Sky Shader
	SHADER_SKIN					= 14, // Skin Shader
	SHADER_WATER				= 17, // Water Shader
	SHADER_LIGHTING30			= 29, // Lighting 3.0 Shader
	SHADER_TILE					= 32, // Tiled Shader
	SHADER_NOLIGHTING			= 33, // No Lighting Shader
};

// An unsigned 32-bit integer, describing the apply mode of a texture.
enum ApplyMode {
	APPLY_REPLACE				= 0, // Replaces existing color
	APPLY_DECAL					= 1, // For placing images on the object like stickers.
	APPLY_MODULATE				= 2, // Modulates existing color. (Default)
	APPLY_HILIGHT				= 3, // PS2 Only.  Function Unknown.
	APPLY_HILIGHT2				= 4, // Parallax Flag in some Oblivion meshes.
};

// The motion system. 4 (Box) is used for everything movable. 7 (Keyframed) is used on statics and animated stuff.
enum MotionSystem {
	MO_SYS_INVALID				= 0, // Invalid
	MO_SYS_DYNAMIC				= 1, // A fully-simulated, movable rigid body. At construction time the engine checks the input inertia and selects MO_SYS_SPHERE_INERTIA or MO_SYS_BOX_INERTIA as appropriate.
	MO_SYS_SPHERE				= 2, // Simulation is performed using a sphere inertia tensor.
	MO_SYS_SPHERE_INERTIA		= 3, // This is the same as MO_SYS_SPHERE_INERTIA, except that simulation of the rigid body is "softened".
	MO_SYS_BOX					= 4, // Simulation is performed using a box inertia tensor.
	MO_SYS_BOX_STABILIZED		= 5, // This is the same as MO_SYS_BOX_INERTIA, except that simulation of the rigid body is "softened".
	MO_SYS_KEYFRAMED			= 6, // Simulation is not performed as a normal rigid body. The keyframed rigid body has an infinite mass when viewed by the rest of the system. (used for creatures)
	MO_SYS_FIXED				= 7, // This motion type is used for the static elements of a game scene, e.g. the landscape. Faster than MO_SYS_KEYFRAMED at velocity 0. (used for weapons)
	MO_SYS_THIN_BOX				= 8, // A box inertia motion which is optimized for thin boxes and has less stability problems
	MO_SYS_CHARACTER			= 9, // A specialized motion used for character controllers
};

// Determines the way the billboard will react to the camera.
enum BillboardMode {
	ALWAYS_FACE_CAMERA			= 0, // The billboard will always face the camera.
	ROTATE_ABOUT_UP				= 1, // The billboard will only rotate around the up axis.
	RIGID_FACE_CAMERA			= 2, // Rigid Face Camera.
	ALWAYS_FACE_CENTER			= 3, // Always Face Center.
	RIGID_FACE_CENTER			= 4, // Rigid Face Center.
	ROTATE_ABOUT_UP2			= 9, // The billboard will only rotate around the up axis (same as ROTATE_ABOUT_UP?).
};

// This enum contains the options for doing z buffer tests.
enum ZCompareMode {
	ZCOMP_ALWAYS				= 0, // Test will allways succeed. The Z Buffer value is ignored.
	ZCOMP_LESS					= 1, // The test will only succeed if the pixel is nearer than the previous pixel.
	ZCOMP_EQUAL					= 2, // Test will only succeed if the z value of the pixel to be drawn is equal to the value of the previous drawn pixel.
	ZCOMP_LESS_EQUAL			= 3, // Test will succeed if the z value of the pixel to be drawn is smaller than or equal to the value in the Z Buffer.
	ZCOMP_GREATER				= 4, // Opposite of TEST_LESS.
	ZCOMP_NOT_EQUAL				= 5, // Test will succeed if the z value of the pixel to be drawn is NOT equal to the value of the previously drawn pixel.
	ZCOMP_GREATER_EQUAL			= 6, // Opposite of TEST_LESS_EQUAL.
	ZCOMP_NEVER					= 7, // Test will allways return false. Nothing is drawn at all.
};

// Used by NiPoint3InterpControllers to select which type of color in the controlled object that will be animated.
enum TargetColor {
	TC_AMBIENT					= 0, // Control the ambient color.
	TC_DIFFUSE					= 1, // Control the diffuse color.
	TC_SPECULAR					= 2, // Control the specular color.
	TC_SELF_ILLUM				= 3, // Control the self illumination color.
};

enum SkyrimHavokMaterial {
	SKY_HAV_MAT_LIGHT_WOOD					= 365420259,	// Light Wood
	SKY_HAV_MAT_BROKEN_STONE				= 131151687,	// Broken Stone
	SKY_HAV_MAT_SNOW						= 398949039,	// Snow
	SKY_HAV_MAT_GRAVEL						= 428587608,	// Gravel
	SKY_HAV_MAT_MATERIAL_CHAIN_METAL		= 438912228,	// Material Chain Metal
	SKY_HAV_MAT_WOOD						= 500811281,	// Wood
	SKY_HAV_MAT_SKIN						= 591247106,	// Skin
	SKY_HAV_MAT_BARREL						= 732141076,	// Barrel
	SKY_HAV_MAT_MATERIAL_CERAMIC_MEDIUM		= 781661019,	// Material Ceramic Medium
	SKY_HAV_MAT_MATERIAL_BASKET				= 790784366,	// Material Basket
	SKY_HAV_MAT_ICE							= 873356572,	// Ice
	SKY_HAV_MAT_STAIRS_STONE				= 899511101,	// Stairs Stone
	SKY_HAV_MAT_MATERIAL_BLADE_1HAND		= 1060167844,	// Material Blade 1 Hand
	SKY_HAV_MAT_WATER						= 1024582599,	// Water
	SKY_HAV_MAT_UNKNOWN_1028101969			= 1028101969,	// Unknown in Creation Kit v1.6.89.0. Found in actors\draugr\character assets\skeletons.nif.
	SKY_HAV_MAT_MATERIAL_BOOK				= 1264672850,	// Material Book
	SKY_HAV_MAT_SOLID_METAL					= 1288358971,	// Solid Metal
	SKY_HAV_MAT_MATERIAL_AXE_1HAND			= 1305674443,	// Material Axe 1Hand
	SKY_HAV_MAT_UNKNOWN_1440721808			= 1440721808,	// Unknown in Creation Kit v1.6.89.0. Found in armor\draugr\draugrbootsfemale_go.nif or armor\amuletsandrings\amuletgnd.nif.
	SKY_HAV_MAT_STAIRS_WOOD					= 1461712277,	// Stairs Wood
	SKY_HAV_MAT_MUD							= 1486385281,	// Mud
	SKY_HAV_MAT_STAIRS_SNOW					= 1560365355,	// Stairs Snow
	SKY_HAV_MAT_UNKNOWN_1574477864			= 1574477864,	// Unknown in Creation Kit v1.6.89.0. Found in actors\dragon\character assets\skeleton.nif.
	SKY_HAV_MAT_UNKNOWN_1591009235			= 1591009235,	// Unknown in Creation Kit v1.6.89.0. Found in trap objects or clutter\displaycases\displaycaselgangled01.nif or actors\deer\character assets\skeleton.nif.
	SKY_HAV_MAT_MATERIAL_BOWS_STAVES		= 1607128641,	// Material Bows Staves
	SKY_HAV_MAT_MATERIAL_WOOD_AS_STAIRS		= 1803571212,	// Material Wood As Stairs
	SKY_HAV_MAT_GRASS						= 1848600814,	// Grass
	SKY_HAV_MAT_MATERIAL_BOULDER_LARGE		= 1885326971,	// Material Boulder Large
	SKY_HAV_MAT_MATERIAL_STONE_AS_STAIRS	= 1886078335,	// Material Stone As Stairs
	SKY_HAV_MAT_MATERIAL_BLADE_2HAND		= 2022742644,	// Material Blade 2Hand
	SKY_HAV_MAT_MATERIAL_BOTTLE_SMALL		= 2025794648,	// Material Bottle Small
	SKY_HAV_MAT_SAND						= 2168343821,	// Sand
	SKY_HAV_MAT_HEAVY_METAL					= 2229413539,	// Heavy Metal
	SKY_HAV_MAT_DRAGON						= 2518321175,	// Dragon
	SKY_HAV_MAT_MATERIAL_BLADE_1HAND_SMALL	= 2617944780,	// Material Blade 1Hand Small
	SKY_HAV_MAT_MATERIAL_SKIN_SMALL			= 2632367422,	// Material Skin Small
	SKY_HAV_MAT_STAIRS_BROKEN_STONE			= 2892392795,	// Stairs Broken Stone
	SKY_HAV_MAT_MATERIAL_SKIN_LARGE			= 2965929619,	// Material Skin Large
	SKY_HAV_MAT_ORGANIC						= 2974920155,	// Organic
	SKY_HAV_MAT_MATERIAL_BONE				= 3049421844,	// Material Bone
	SKY_HAV_MAT_HEAVY_WOOD					= 3070783559,	// Heavy Wood
	SKY_HAV_MAT_MATERIAL_CHAIN				= 3074114406,	// Material Chain
	SKY_HAV_MAT_DIRT						= 3106094762,	// Dirt
	SKY_HAV_MAT_MATERIAL_ARMOR_LIGHT		= 3424720541,	// Material Armor Light
	SKY_HAV_MAT_MATERIAL_SHIELD_LIGHT		= 3448167928,	// Material Shield Light
	SKY_HAV_MAT_MATERIAL_COIN				= 3589100606,	// Material Coin
	SKY_HAV_MAT_MATERIAL_SHIELD_HEAVY		= 3702389584,	// Material Shield Heavy
	SKY_HAV_MAT_MATERIAL_ARMOR_HEAVY		= 3708432437,	// Material Armor Heavy
	SKY_HAV_MAT_MATERIAL_ARROW				= 3725505938,	// Material Arrow
	SKY_HAV_MAT_GLASS						= 3739830338,	// Glass
	SKY_HAV_MAT_STONE						= 3741512247,	// Stone
	SKY_HAV_MAT_CLOTH						= 3839073443,	// Cloth
	SKY_HAV_MAT_MATERIAL_BLUNT_2HAND		= 3969592277,	// Material Blunt 2Hand
	SKY_HAV_MAT_MATERIAL_BOULDER_MEDIUM		= 4283869410,	// Material Boulder Medium
};

enum ChannelConvention {
	CC_FIXED					= 0, // Fixed
	CC_INDEX					= 3, // Palettized
	CC_COMPRESSED				= 4, // Compressed
	CC_EMPTY					= 5, // Empty
};

// Determines how the raw image data is stored in NiRawImageData.
enum ImageType {
	RGB							= 1, // Colors store red, blue, and green components.
	RGBA						= 2, // Colors store red, blue, green, and alpha components.
};

// An unsigned 32-bit integer, describing the color depth of a texture.
enum PixelLayout {
	PIX_LAY_PALETTISED			= 0, // Texture is in 8-bit paletized format.
	PIX_LAY_HIGH_COLOR_16		= 1, // Texture is in 16-bit high color format.
	PIX_LAY_TRUE_COLOR_32		= 2, // Texture is in 32-bit true color format.
	PIX_LAY_COMPRESSED			= 3, // Texture is compressed.
	PIX_LAY_BUMPMAP				= 4, // Texture is a grayscale bump map.
	PIX_LAY_PALETTISED_4		= 5, // Texture is in 4-bit paletized format.
	PIX_LAY_DEFAULT				= 6, // Use default setting.
};

// Used by NiGeometryData to control the volatility of the mesh.  While they appear to be flags they behave as an enum.
enum ConsistencyType {
	CT_MUTABLE					= 0x0000, // Mutable Mesh
	CT_STATIC					= 0x4000, // Static Mesh
	CT_VOLATILE					= 0x8000, // Volatile Mesh
};

// Specifies the availiable texture clamp modes.  That is, the behavior of pixels outside the range of the texture.
enum TexClampMode {
	CLAMP_S_CLAMP_T				= 0, // Clamp in both directions.
	CLAMP_S_WRAP_T				= 1, // Clamp in the S(U) direction but wrap in the T(V) direction.
	WRAP_S_CLAMP_T				= 2, // Wrap in the S(U) direction but clamp in the T(V) direction.
	WRAP_S_WRAP_T				= 3, // Wrap in both directions.
};

// The data format of components.
enum ComponentFormat {
	F_UNKNOWN					= 0x00000000, // Unknown, or don't care, format.
	F_INT8_1					= 0x00010101, // F_INT8_1
	F_INT8_2					= 0x00020102, // F_INT8_2
	F_INT8_3					= 0x00030103, // F_INT8_3
	F_INT8_4					= 0x00040104, // F_INT8_4
	F_UINT8_1					= 0x00010105, // F_UINT8_1
	F_UINT8_2					= 0x00020106, // F_UINT8_2
	F_UINT8_3					= 0x00030107, // F_UINT8_3
	F_UINT8_4					= 0x00040108, // F_UINT8_4
	F_NORMINT8_1				= 0x00010109, // F_NORMINT8_1
	F_NORMINT8_2				= 0x0002010A, // F_NORMINT8_2
	F_NORMINT8_3				= 0x0003010B, // F_NORMINT8_3
	F_NORMINT8_4				= 0x0004010C, // F_NORMINT8_4
	F_NORMUINT8_1				= 0x0001010D, // F_NORMUINT8_1
	F_NORMUINT8_2				= 0x0002010E, // F_NORMUINT8_2
	F_NORMUINT8_3				= 0x0003010F, // F_NORMUINT8_3
	F_NORMUINT8_4				= 0x00040110, // F_NORMUINT8_4
	F_INT16_1					= 0x00010211, // F_INT16_1
	F_INT16_2					= 0x00020212, // F_INT16_2
	F_INT16_3					= 0x00030213, // F_INT16_3
	F_INT16_4					= 0x00040214, // F_INT16_4
	F_UINT16_1					= 0x00010215, // F_UINT16_1
	F_UINT16_2					= 0x00020216, // F_UINT16_2
	F_UINT16_3					= 0x00030217, // F_UINT16_3
	F_UINT16_4					= 0x00040218, // F_UINT16_4
	F_NORMINT16_1				= 0x00010219, // F_NORMINT16_1
	F_NORMINT16_2				= 0x0002021A, // F_NORMINT16_2
	F_NORMINT16_3				= 0x0003021B, // F_NORMINT16_3
	F_NORMINT16_4				= 0x0004021C, // F_NORMINT16_4
	F_NORMUINT16_1				= 0x0001021D, // F_NORMUINT16_1
	F_NORMUINT16_2				= 0x0002021E, // F_NORMUINT16_2
	F_NORMUINT16_3				= 0x0003021F, // F_NORMUINT16_3
	F_NORMUINT16_4				= 0x00040220, // F_NORMUINT16_4
	F_INT32_1					= 0x00010421, // F_INT32_1
	F_INT32_2					= 0x00020422, // F_INT32_2
	F_INT32_3					= 0x00030423, // F_INT32_3
	F_INT32_4					= 0x00040424, // F_INT32_4
	F_UINT32_1					= 0x00010425, // F_UINT32_1
	F_UINT32_2					= 0x00020426, // F_UINT32_2
	F_UINT32_3					= 0x00030427, // F_UINT32_3
	F_UINT32_4					= 0x00040428, // F_UINT32_4
	F_NORMINT32_1				= 0x00010429, // F_NORMINT32_1
	F_NORMINT32_2				= 0x0002042A, // F_NORMINT32_2
	F_NORMINT32_3				= 0x0003042B, // F_NORMINT32_3
	F_NORMINT32_4				= 0x0004042C, // F_NORMINT32_4
	F_NORMUINT32_1				= 0x0001042D, // F_NORMUINT32_1
	F_NORMUINT32_2				= 0x0002042E, // F_NORMUINT32_2
	F_NORMUINT32_3				= 0x0003042F, // F_NORMUINT32_3
	F_NORMUINT32_4				= 0x00040430, // F_NORMUINT32_4
	F_FLOAT16_1					= 0x00010231, // F_FLOAT16_1
	F_FLOAT16_2					= 0x00020232, // F_FLOAT16_2
	F_FLOAT16_3					= 0x00030233, // F_FLOAT16_3
	F_FLOAT16_4					= 0x00040234, // F_FLOAT16_4
	F_FLOAT32_1					= 0x00010435, // F_FLOAT32_1
	F_FLOAT32_2					= 0x00020436, // F_FLOAT32_2
	F_FLOAT32_3					= 0x00030437, // F_FLOAT32_3
	F_FLOAT32_4					= 0x00040438, // F_FLOAT32_4
	F_UINT_10_10_10_L1			= 0x00010439, // F_UINT_10_10_10_L1
	F_NORMINT_10_10_10_L1		= 0x0001043A, // F_NORMINT_10_10_10_L1
	F_NORMINT_11_11_10			= 0x0001043B, // F_NORMINT_11_11_10
	F_NORMUINT8_4_BGRA			= 0x0004013C, // F_NORMUINT8_4_BGRA
	F_NORMINT_10_10_10_2		= 0x0001043D, // F_NORMINT_10_10_10_2
	F_UINT_10_10_10_2			= 0x0001043E, // F_UINT_10_10_10_2
};

// The motion type. Determines quality of motion?
enum MotionQuality {
	MO_QUAL_INVALID				= 0, // Automatically assigned to MO_QUAL_FIXED, MO_QUAL_KEYFRAMED or MO_QUAL_DEBRIS
	MO_QUAL_FIXED				= 1, // Use this for fixed bodies.
	MO_QUAL_KEYFRAMED			= 2, // Use this for moving objects with infinite mass.
	MO_QUAL_DEBRIS				= 3, // Use this for all your debris objects
	MO_QUAL_MOVING				= 4, // Use this for moving bodies, which should not leave the world.
	MO_QUAL_CRITICAL			= 5, // Use this for all objects, which you cannot afford to tunnel through the world at all
	MO_QUAL_BULLET				= 6, // Use this for very fast objects
	MO_QUAL_USER				= 7, // For user.
	MO_QUAL_CHARACTER			= 8, // Use this for rigid body character controllers
	MO_QUAL_KEYFRAMED_REPORT	= 9, // Use this for moving objects with infinite mass which should report contact points and Toi-collisions against all other bodies, including other fixed and keyframed bodies.
};

// Sets how objects are to be cloned
enum CloningBehavior {
	CLONING_SHARE				= 0, // Share this object pointer with the newly cloned scene.
	CLONING_COPY				= 1, // Create an exact duplicate of this object for use with the newly cloned scene.
	CLONING_BLANK_COPY			= 2, // Create a copy of this object for use with the newly cloned stream, leaving some of the data to be written later.
};

enum PropagationMode {
	PROPAGATE_ON_SUCCESS		= 0, // On Success
	PROPAGATE_ON_FAILURE		= 1, // On Failure
	PROPAGATE_ALWAYS			= 2, // Always
	PROPAGATE_NEVER				= 3, // Never
};

// Specifies the pixel format used by the NiPixelData object to store a texture.
enum PixelFormat {
	PX_FMT_RGB8					= 0, // 24-bit color: uses 8 bit to store each red, blue, and green component.
	PX_FMT_RGBA8				= 1, // 32-bit color with alpha: uses 8 bits to store each red, blue, green, and alpha component.
	PX_FMT_PAL8					= 2, // 8-bit palette index: uses 8 bits to store an index into the palette stored in a NiPalette object.
	PX_FMT_DXT1					= 4, // DXT1 compressed texture.
	PX_FMT_DXT5					= 5, // DXT5 compressed texture.
	PX_FMT_DXT5_ALT				= 6, // DXT5 compressed texture. It is not clear what the difference is with PX_FMT_DXT5.
};

// The animation cyle behavior.
enum CycleType {
	CYCLE_LOOP					= 0, // Loop
	CYCLE_REVERSE				= 1, // Reverse
	CYCLE_CLAMP					= 2, // Clamp
};

// The force field's type.
enum FieldType {
	FIELD_WIND					= 0, // Wind (fixed direction)
	FIELD_POINT					= 1, // Point (fixed origin)
};

// Values for configuring the shader type in a BSLightingShaderProperty
enum BSLightingShaderPropertyShaderType {
	DEFAULT						= 0, // Default
	ENVIRONMENT_MAP				= 1, // Enables EnvMap Mask(TS6), EnvMap Scale
	GLOW_SHADER					= 2, // Enables Glow(TS3)
	HEIGHTMAP					= 3, // Enables Height(TS4)
	FACE_TINT					= 4, // Enables SubSurface(TS3), Detail(TS4), Tint(TS7)
	SKIN_TINT					= 5, // Enables Skin Tint Color
	HAIR_TINT					= 6, // Enables Hair Tint Color
	PARALLAX_OCC_MATERIAL		= 7, // Enables Height(TS4), Max Passes, Scale.  Unused?
	WORLD_MULTITEXTURE			= 8, // World Multitexture
	WORLDMAP1					= 9, // WorldMap1
	UNKNOWN_10					= 10, // Unknown 10
	MULTILAYER_PARALLAX			= 11, // Enables EnvMap Mask(TS6), Layer(TS7), Parallax Layer Thickness, Parallax Refraction Scale, Parallax Inner Layer U Scale, Parallax Inner Layer V Scale, EnvMap Scale
	UNKNOWN_12					= 12, // Unknown 12
	WORLDMAP2					= 13, // WorldMap2
	SPARKLE_SNOW				= 14, // Enables SparkleParams
	WORLDMAP3					= 15, // WorldMap3
	EYE_ENVMAP					= 16, // Enables EnvMap Mask(TS6), Eye EnvMap Scale
	UNKNOWN_17					= 17, // Unknown 17
	WORLDMAP4					= 18, // WorldMap4
	WORLD_LOD_MULTITEXTURE		= 19, // World LOD Multitexture
};

// Determines decay function.  Used by NiPSysBombModifier.
enum DecayType {
	DECAY_NONE					= 0, // No decay.
	DECAY_LINEAR				= 1, // Linear decay.
	DECAY_EXPONENTIAL			= 2, // Exponential decay.
};

// Controls which parts of the mesh that the particles are emitted from.
enum EmitFrom {
	EMIT_FROM_VERTICES			= 0, // Emit particles from the vertices of the mesh.
	EMIT_FROM_FACE_CENTER		= 1, // Emit particles from the center of the faces of the mesh.
	EMIT_FROM_EDGE_CENTER		= 2, // Emit particles from the center of the edges of the mesh.
	EMIT_FROM_FACE_SURFACE		= 3, // Perhaps randomly emit particles from anywhere on the faces of the mesh?
	EMIT_FROM_EDGE_SURFACE		= 4, // Perhaps randomly emit particles from anywhere on the edges of the mesh?
};

// Skyrim water shader property flags
enum SkyrimWaterShaderFlags {
	SWSF1_UNKNOWN0				= 1, // Unknown
	SWSF1_BYPASS_REFRACTION_MAP	= 2, // Bypasses refraction map when set to 1
	SWSF1_WATER_TOGGLE			= 4, // Main water Layer on/off
	SWSF1_UNKNOWN3				= 8, // Unknown
	SWSF1_UNKNOWN4				= 16, // Unknown
	SWSF1_UNKNOWN5				= 32, // Unknown
	SWSF1_HIGHLIGHT_LAYER_TOGGLE= 64, // Reflection layer 2 on/off. (is this scene reflection?)
	SWSF1_ENABLED				= 128, // Water layer on/off
};

// Skyrim Shader Property Flags 1
enum SkyrimShaderPropertyFlags1 {
	SLSF1_SPECULAR					= 1,			// Enables Specularity
	SLSF1_SKINNED					= 2,			// Required For Skinned Meshes.
	SLSF1_TEMP_REFRACTION			= 4,			// Unknown
	SLSF1_VERTEX_ALPHA				= 8,			// Enables using alpha component of vertex colors.
	SLSF1_GREYSCALE_TO_PALETTECOLOR	= 16,			// in EffectShaderProperty
	SLSF1_GREYSCALE_TO_PALETTEALPHA	= 32,			// in EffectShaderProperty
	SLSF1_USE_FALLOFF				= 64,			// Use Falloff value in EffectShaderProperty
	SLSF1_ENVIRONMENT_MAPPING		= 128,			// Environment mapping (uses Envmap Scale).
	SLSF1_RECIEVE_SHADOWS			= 256,			// Object can recieve shadows.
	SLSF1_CAST_SHADOWS				= 512,			// Can cast shadows
	SLSF1_FACEGEN_DETAIL_MAP		= 1024,			// Use a face detail map in the 4th texture slot.
	SLSF1_PARALLAX					= 2048,			// Unused?
	SLSF1_MODEL_SPACE_NORMALS		= 4096,			// Use Model space normals and an external Specular Map.
	SLSF1_NON_PROJECTIVE_SHADOWS	= 8192,			// Unknown.
	SLSF1_LANDSCAPE					= 16384,		// Unknown.
	SLSF1_REFRACTION				= 32768,		// Use normal map for refraction effect.
	SLSF1_FIRE_REFRACTION			= 65536,		// SLSF1_Fire_Refraction
	SLSF1_EYE_ENVIRONMENT_MAPPING	= 131072,		// Eye Environment Mapping (Must use the Eye shader and the model must be skinned)
	SLSF1_HAIR_SOFT_LIGHTING		= 262144,		// Keeps from going too bright under lights (hair shader only)
	SLSF1_SCREENDOOR_ALPHA_FADE		= 524288,		// SLSF1_Screendoor_Alpha_Fade
	SLSF1_LOCALMAP_HIDE_SECRET		= 1048576,		// Object and anything it is positioned above will not render on local map view.
	SLSF1_FACEGEN_RGB_TINT			= 2097152,		// Use tintmask for Face.
	SLSF1_OWN_EMIT					= 4194304,		// Provides its own emittance color. (will not absorb light/ambient color?)
	SLSF1_PROJECTED_UV				= 8388608,		// Used for decalling?
	SLSF1_MULTIPLE_TEXTURES			= 16777216,		// SLSF1_Multiple_Textures
	SLSF1_REMAPPABLE_TEXTURES		= 33554432,		// SLSF1_Remappable_Textures
	SLSF1_DECAL						= 67108864,		// SLSF1_Decal
	SLSF1_DYNAMIC_DECAL				= 134217728,	// SLSF1_Dynamic_Decal
	SLSF1_PARALLAX_OCCLUSION		= 268435456,	// SLSF1_Parallax_Occlusion
	SLSF1_EXTERNAL_EMITTANCE		= 536870912,	// SLSF1_External_Emittance
	SLSF1_SOFT_EFFECT				= 1073741824,	// SLSF1_Soft_Effect
	SLSF1_ZBUFFER_TEST				= 2147483648,	// ZBuffer Test (1=on)
};

// Shader Property Flags
enum BSShaderFlags {
	SF_SPECULAR						= 1,			// Enables Specularity
	SF_SKINNED						= 2,			// Required For Skinned Meshes
	SF_LOWDETAIL					= 4,			// Lowddetail (seems to use standard diff/norm/spec shader)
	SF_VERTEX_ALPHA					= 8,			// Vertex Alpha
	SF_UNKNOWN_1					= 16,			// Unknown
	SF_SINGLE_PASS					= 32,			// Single Pass
	SF_EMPTY						= 64,			// Unknown
	SF_ENVIRONMENT_MAPPING			= 128,			// Environment mapping (uses Envmap Scale)
	SF_ALPHA_TEXTURE				= 256,			// Alpha Texture Requires NiAlphaProperty to Enable
	SF_UNKNOWN_2					= 512,			// Unknown
	SF_FACEGEN						= 1024,			// FaceGen
	SF_PARALLAX_SHADER_INDEX_15		= 2048,			// Parallax
	SF_UNKNOWN_3					= 4096,			// Unknown/Crash
	SF_NON_PROJECTIVE_SHADOWS		= 8192,			// Non-Projective Shadows
	SF_UNKNOWN_4					= 16384,		// Unknown/Crash
	SF_REFRACTION					= 32768,		// Refraction (switches on refraction power)
	SF_FIRE_REFRACTION				= 65536,		// Fire Refraction (switches on refraction power/period)
	SF_EYE_ENVIRONMENT_MAPPING		= 131072,		// Eye Environment Mapping (does not use envmap light fade or envmap scale)
	SF_HAIR							= 262144,		// Hair
	SF_DYNAMIC_ALPHA				= 524288,		// Dynamic Alpha
	SF_LOCALMAP_HIDE_SECRET			= 1048576,		// Localmap Hide Secret
	SF_WINDOW_ENVIRONMENT_MAPPING	= 2097152,		// Window Environment Mapping
	SF_TREE_BILLBOARD				= 4194304,		// Tree Billboard
	SF_SHADOW_FRUSTUM				= 8388608,		// Shadow Frustum
	SF_MULTIPLE_TEXTURES			= 16777216,		// Multiple Textures (base diff/norm become null)
	SF_REMAPPABLE_TEXTURES			= 33554432,		// usually seen w/texture animation
	SF_DECAL_SINGLE_PASS			= 67108864,		// Decal
	SF_DYNAMIC_DECAL_SINGLE_PASS	= 134217728,	// Dynamic Decal
	SF_PARALLAX_OCCULSION			= 268435456,	// Parallax Occlusion
	SF_EXTERNAL_EMITTANCE			= 536870912,	// External Emittance
	SF_SHADOW_MAP					= 1073741824,	// Shadow Map
	SF_ZBUFFER_TEST					= 2147483648,	// ZBuffer Test (1=on)
};

// Furniture entry points. It specifies the direction(s) from where the actor is able to enter (and leave) the position.
enum FurnitureEntryPoints {
	FRONT						= 1, // front entry point
	BEHIND						= 2, // behind entry point
	RIGHT						= 4, // right entry point
	LEFT						= 8, // left entry point
	UP							= 16, // up entry point - unknown function. Used on some beds in Skyrim, probably for blocking of sleeping position.
};

// Editor flags for the Body Partitions
enum BSPartFlag {
	PF_EDITOR_VISIBLE			= 1,	// Visible in Editor
	PF_START_NET_BONESET		= 256,	// Start a new shared boneset.  It is expected this BoneSet and the following sets in the Skin Partition will have the same bones.
};

// Determines how the data stream is accessed?
enum DataStreamAccess {
	CPU_READ						= 1, // CPU Read
	CPU_WRITE_STATIC				= 2, // CPU Write Static
	CPU_WRITE_MUTABLE				= 4, // CPU Write Mutable
	CPU_WRITE_VOLATILE				= 8, // CPU Write Volatile
	GPU_READ						= 16, // GPU Read
	GPU_WRITE						= 32, // GPU Write
	CPU_WRITE_STATIC_INITITIALIZED	= 64, // CPU Write Static Inititialized
};

// Skyrim Shader Property Flags 2
enum SkyrimShaderPropertyFlags2 {
	SLSF2_ZBUFFER_WRITE					= 1,			// Enables writing to the Z-Buffer
	SLSF2_LOD_LANDSCAPE					= 2,			// SLSF2_LOD_Landscape
	SLSF2_LOD_OBJECTS					= 4,			// SLSF2_LOD_Objects
	SLSF2_NO_FADE						= 8,			// SLSF2_No_Fade
	SLSF2_DOUBLE_SIDED					= 16,			// Double-sided rendering.
	SLSF2_VERTEX_COLORS					= 32,			// Has Vertex Colors.
	SLSF2_GLOW_MAP						= 64,			// Use Glow Map in the third texture slot.
	SLSF2_ASSUME_SHADOWMASK				= 128,			// SLSF2_Assume_Shadowmask
	SLSF2_PACKED_TANGENT				= 256,			// SLSF2_Packed_Tangent
	SLSF2_MULTI_INDEX_SNOW				= 512,			// SLSF2_Multi_Index_Snow
	SLSF2_VERTEX_LIGHTING				= 1024,			// SLSF2_Vertex_Lighting
	SLSF2_UNIFORM_SCALE					= 2048,			// SLSF2_Uniform_Scale
	SLSF2_FIT_SLOPE						= 4096,			// SLSF2_Fit_Slope
	SLSF2_BILLBOARD						= 8192,			// SLSF2_Billboard
	SLSF2_NO_LOD_LAND_BLEND				= 16384,		// SLSF2_No_LOD_Land_Blend
	SLSF2_ENVMAP_LIGHT_FADE				= 32768,		// SLSF2_EnvMap_Light_Fade
	SLSF2_WIREFRAME						= 65536,		// Wireframe (Seems to only work on particles)
	SLSF2_WEAPON_BLOOD					= 131072,		// Used for blood decals on weapons.
	SLSF2_HIDE_ON_LOCAL_MAP				= 262144,		// Similar to hide secret, but only for self?
	SLSF2_PREMULT_ALPHA					= 524288,		// Has Premultiplied Alpha
	SLSF2_CLOUD_LOD						= 1048576,		// SLSF2_Cloud_LOD
	SLSF2_ANISOTROPIC_LIGHTING			= 2097152,		// Hair only?
	SLSF2_NO_TRANSPARENCY_MULTISAMPLING	= 4194304,		// SLSF2_No_Transparency_Multisampling
	SLSF2_UNUSED01						= 8388608,		// Unused?
	SLSF2_MULTI_LAYER_PARALLAX			= 16777216,		// Use Multilayer (inner-layer) Map
	SLSF2_SOFT_LIGHTING					= 33554432,		// Use Soft Lighting Map
	SLSF2_RIM_LIGHTING					= 67108864,		// Use Rim Lighting Map
	SLSF2_BACK_LIGHTING					= 134217728,	// Use Back Lighting Map
	SLSF2_UNUSED02						= 268435456,	// Unused?
	SLSF2_TREE_ANIM						= 536870912,	// Enables Vertex Animation, Flutter Animation
	SLSF2_EFFECT_LIGHTING				= 1073741824,	// SLSF2_Effect_Lighting
	SLSF2_HD_LOD_OBJECTS				= 2147483648,	// SLSF2_HD_LOD_Objects
};

// An unsigned 32-bit integer, describing what's inside the segment
enum BSSegmentFlags {
	BSSEG_WATER		= 512, // Contains water.
};

struct TexCoord {
	float u, v;
};
struct Triangle {
	uint16 v1, v2, v3;
};
struct Vector3 {
	float	x, y, z;
};
struct Vector4 {
	float	x, y, z, w;
};
struct Float2 {
	float data[2];
};
struct Matrix22 {
	Float2 rows[2];
};
struct Float3 {
	float data[3];
};
struct Matrix33 {
	Float3 rows[3];
};
struct Float4 {
	float data[4];
};
struct Matrix44 {
	Float4 rows[4];
};
struct Color3 {
	float r, g, b;
};
struct Color4 {
	float r, g, b, a;
};
struct Quaternion {
	float w, x, y, z;
};
struct InertiaMatrix {
	Float4 rows[3];
};

struct BoundingBox {
	uint32		unknownInt;
	Vector3		translation;
	Matrix33	rotation;
	Vector3		radius;
};

typedef string IndexString, Char8String, HeaderString, ShortString, LineString;
typedef uint8	byte;

struct NifInfo {
	uint32		version;
	uint32		userVersion;
	uint32		userVersion2;
	EndianType	endian;
	string		creator;
	string		exportInfo1;
	string		exportInfo2;

	NifInfo(uint32 _version = VER_4_0_0_2, uint32 _userVersion = 0, uint32 _userVersion2 = 0) : version(_version), userVersion(_userVersion), userVersion2(_userVersion2), endian(ENDIAN_LITTLE) {}
};

// Used to enable static arrays to be members of vectors
template<int size, class T> struct array {
	array() {
		for (size_t i = 0; i < size; ++i)
			data[i] = T();
	}
	array(size_t n, T t0) {
		data[0] = t0;
		for (size_t i = 1; i < size; ++i)
			data[i] = T();
	}
	array(size_t n, T t0, T t1) {
		data[0] = t0;
		data[1] = t1;
		for (size_t i = 2; i < size; ++i)
			data[i] = T();
	}
	array(size_t n, T t0, T t1, T t2) {
		data[0] = t0;
		data[1] = t1;
		data[2] = t2;
		for (size_t i = 3; i < size; ++i)
			data[i] = T();
	}
	array(size_t n, T t0, T t1, T t2, T t3) {
		data[0] = t0;
		data[1] = t1;
		data[2] = t2;
		data[3] = t3;
		for (size_t i = 4; i < size; ++i)
			data[i] = T();
	}
	array(size_t n, T t0, T t1, T t2, T t3, T t4) {
		data[0] = t0;
		data[1] = t1;
		data[2] = t2;
		data[3] = t3;
		data[4] = t4;
		for (size_t i = 5; i < size; ++i)
			data[i] = T();
	}
	array(size_t n, T t0, T t1, T t2, T t3, T t4, T t5) {
		data[0] = t0;
		data[1] = t1;
		data[2] = t2;
		data[3] = t3;
		data[4] = t4;
		data[5] = t5;
		for (size_t i = 6; i < size; ++i)
			data[i] = T();
	}
	array(size_t n, T t0, T t1, T t2, T t3, T t4, T t5, T t6) {
		data[0] = t0;
		data[1] = t1;
		data[2] = t2;
		data[3] = t3;
		data[4] = t4;
		data[5] = t5;
		data[6] = t6;
		for (size_t i = 7; i < size; ++i)
			data[i] = T();
	}
	~array() {}
	T & operator[](uint32 index) {
		return data[index];
	}
	const T & operator[](uint32 index) const {
		return data[index];
	}
private:
	T data[size];
};

typedef uint16 Flags;

// Smart Pointer Template
template<class T> class Ref {
public:
	Ref(T *object = NULL) : _object(object) { if (_object) _object->AddRef(); }
	Ref(const Ref & ref_to_copy) { _object = ref_to_copy._object; if (_object) _object->AddRef(); }
	~Ref() { if (_object) _object->SubtractRef(); }

	operator T*()						const { return _object; }
	T*		operator->()				const { return _object; }
	T&		operator*()					const { return *_object; }
	bool	operator< (const Ref & ref)	const { return _object < ref._object; }
	bool	operator==(T *object)		const { return _object == object; }
	bool	operator!=(T *object)		const { return _object != object; }
	bool	operator==(const Ref & ref)	const { return _object == ref._object; }
	bool	operator!=(const Ref & ref)	const { return _object != ref._object; }

	Ref&	operator=(T *object) {
		if (_object != object) {
			if (object)
				object->AddRef();
			if (_object)
				_object->SubtractRef();
			_object = object;
		}
		return *this;
	}

	Ref&	operator=(const Ref &ref) {
		if (_object != ref._object) {
			if (ref._object)
				ref._object->AddRef();
			if (_object)
				_object->SubtractRef();
			_object = ref._object;
		}
		return *this;
	}

protected:
	T *_object;
};

class NiObject;
typedef Ref<NiObject> NiObjectRef;

typedef NiObject * (*obj_factory_func)();

class ObjectRegistry {
	static map<string, obj_factory_func> object_map;
public:
	static void		RegisterObject(const char *type_name, obj_factory_func create_func) {
		object_map[type_name] = create_func;
	}
	static NiObject	*CreateObject(const char *type_name) {
		auto	i = object_map.find(type_name);
		if (i != object_map.end())
			return (*i)();
		return NULL;
	}
};

map<string, obj_factory_func> ObjectRegistry::object_map;

/**
 * Run Time Type Information Class
 */
class Type {
	string name;
	static int num_types;
public:
	Type(const char *name, const Type *base_type) : name(name), base_type(base_type), internal_type_number(num_types++) {}

	string GetTypeName() const { return name; }

	bool IsSameType(const Type & compare_to) const { return &compare_to == this; }
	bool IsDerivedType(const Type & compare_to) const {
		for (const Type * search = this; search; search = search->base_type) {
			if (search == &compare_to)
				return true;
		}
		return false;
	}
	bool operator<(const Type & compare_to) const { return (this < &compare_to); }
	NiObject *Create() const { return ObjectRegistry::CreateObject(name); }
	const Type *base_type;
	const int internal_type_number;
};

int Type::num_types;

class RefObject {
	mutable uint32 _ref_count;
	static uint32 objectsInMemory;

public:
	static const Type TYPE;
	static uint32 NumObjectsInMemory() { return objectsInMemory; }

	RefObject() { _ref_count = 0; objectsInMemory++; }
	RefObject(const RefObject& src) { _ref_count = 0; objectsInMemory++; }
	virtual ~RefObject() { objectsInMemory--; }

	virtual string asString(bool verbose = false) const = 0;
	virtual const Type &GetType() const { return TYPE; }
	virtual string GetIDString() const { return str("(") + GetType().GetTypeName() + ")"; }

	bool	IsSameType(const Type & compare_to)		const	{ return GetType().IsSameType(compare_to); }
	bool	IsSameType(const RefObject *object)		const	{ return GetType().IsSameType(object->GetType()); }
	bool	IsDerivedType(const Type & compare_to)	const	{ return GetType().IsDerivedType(compare_to); }
	bool	IsDerivedType(const RefObject *object)	const	{ return GetType().IsDerivedType(object->GetType()); }
	void	AddRef()								const	{ ++_ref_count; }
	void	SubtractRef()							const	{ if (--_ref_count < 1) delete this; }
	uint32	GetNumRefs()									{ return _ref_count; }

	virtual void Read(istream_ref in, list<uint32> & link_stack, const NifInfo & info) = 0;
	virtual void Write(ostream_ref out, const map< Ref<NiObject>, uint32> & link_map, list<NiObject*> & missing_link_stack, const NifInfo & info) const = 0;
	virtual void FixLinks(const map<uint32, Ref<NiObject> > & objects, list<uint32> & link_stack, list<Ref<NiObject> > & missing_link_stack, const NifInfo & info) = 0;
	virtual list< Ref<NiObject> > GetRefs() const = 0;
};

const Type RefObject::TYPE("RefObject", 0);


class NiObject : public RefObject {
public:
	static NiObject*	Create()										{ return new NiObject; }
	string				asString(bool verbose = false)	const override	{ return ""; }
	Ref<NiObject> Clone(uint32 version = 0xFFFFFFFF, uint32 user_version = 0) {
		dynamic_memory_writer		mout;
		NiObjectRef			clone = ObjectRegistry::CreateObject(GetType().GetTypeName());
		map<NiObjectRef, uint32> link_map;

		NifInfo				info(version, user_version);
		list<NiObject*>		missing_link_stack;
		Write(mout, link_map, missing_link_stack, info);

		list<uint32>		link_stack;
		memory_reader		min(mout.data());
		clone->Read(min, link_stack, info);
		return clone;
	}
public:
	int internal_block_number;
public:
	void Read(istream_ref in, list<uint32> & link_stack, const NifInfo & info) override {}
	void Write(ostream_ref out, const map<NiObjectRef, uint32> & link_map, list<NiObject*> & missing_link_stack, const NifInfo & info) const override {}
	void FixLinks(const map<uint32, NiObjectRef> & objects, list<uint32> & link_stack, list<NiObjectRef> & missing_link_stack, const NifInfo & info) override {}
	list<NiObjectRef>		GetRefs() const override	{ return list<Ref<NiObject> >(); }
	virtual list<NiObject*> GetPtrs() const				{ return list<NiObject*>(); }
};


template<class T> Ref<T> StaticCast(NiObject *object) {
	return (T*)object;
}
template<class T> Ref<const T> StaticCast(const NiObject *object) {
	return (const T*)object;
}
template<class T> Ref<T> DynamicCast(NiObject *object) {
	return object && object->IsDerivedType(T::TYPE) ? (T*)object : 0;
}
template<class T> Ref<const T> DynamicCast(const NiObject *object) {
	return object && object->IsDerivedType(T::TYPE) ? (const T*)object : 0;
}

//Classes used

class NiCollisionObject : public NiObject {};
class NiExtraData : public NiObject {};
class NiTimeController : public NiObject {};

class NiObjectNET : public NiObject {
protected:
	BSLightingShaderPropertyShaderType skyrimShaderType;
	IndexString name;
	bool hasOldExtraData;
	IndexString oldExtraPropName;
	unsigned int oldExtraInternalId;
	IndexString oldExtraString;
	byte unknownByte;
	Ref<NiExtraData> extraData;
	mutable unsigned int numExtraDataList;
	dynamic_array<Ref<NiExtraData> > extraDataList;
	Ref<NiTimeController> controller;
public:
	string GetName()	{ return name; }
};

class NiProperty : public NiObjectNET {};

class NiNode;
class NiAVObject : public NiObjectNET {
protected:
	NiNode		*parent;
	uint16		flags;
	uint16		unknownShort1;
	Vector3		translation;
	Matrix33	rotation;
	float		scale;
	Vector3		velocity;
	mutable uint32 numProperties;
	dynamic_array<Ref<NiProperty> > properties;
	array<4, uint32> unknown1;
	byte		unknown2;
	bool		hasBoundingBox;
	BoundingBox	boundingBox;
	Ref<NiCollisionObject> collisionObject;
public:
	Ref<NiNode> GetParent() const	{ return parent; }
};

class NiNode : public NiAVObject {};
class NiSequence : public NiObject {};
class NiControllerSequence : public NiSequence {};

//Bethesda-specific Havok-related
class bhkRefObject : public NiObject {};
class bhkSerializable : public bhkRefObject {};
class bhkWorldObject : public bhkSerializable {};
class bhkEntity : public bhkWorldObject {};

class bhkConstraint : public bhkSerializable {
protected:
	dynamic_array<bhkEntity*> entities;
public:
	dynamic_array<bhkEntity*> GetEntities() const { return entities; }
};

typedef Ref<NiAVObject> NiAVObjectRef;
typedef Ref<NiNode>		NiNodeRef;

struct ExportInfo {
	uint32		unknown;
	ShortString creator;
	ShortString exportInfo1;
	ShortString exportInfo2;
};

struct NifStream {
	istream_ref		stream;
	const NifInfo	&info;

	NifStream(istream_ref stream, const NifInfo &info) : stream(stream), info(info) {}

	template<typename T>	bool	get(T &t) {
		if ((info.endian == ENDIAN_BIG) == iso_bigendian)
			return stream.read(t);
		t = stream.get<typename T_swap_endian_type<T>::type>();
		return true;
	}
	template<typename T>	T		get() {
		if ((info.endian == ENDIAN_BIG) == iso_bigendian)
			return stream.get<T>();
		return stream.get<typename T_swap_endian_type<T>::type>();
	}
};

uint32 ParseVersionString(const char *version) {
	uint32		outver = 0;
	const char	*start = version;
	for (int offset = 3; offset >= 0; --offset) {
		const char *end = string_find(start, '.');
		size_t		len;
		if (!end) {
			if (offset > 0) {
				//This version has only one period in it.  Take the rest of the numbers one character at a time.
				len = 1;
			} else {
				//We've already taken two characters one at a time, so take the rest all at once.
				len = end - start;
			}
		} else {
			len = end - start;
		}

		string_scan	s(start, start + len);
		int	num = s.get();
		if (num > 0xFF)
			return VER_INVALID;

		outver	|= (num << (offset * 8));
		start	+= len + (end ? 1 : 0);
	}

	return outver;
}

uint32 GetVersion(const string &headerString) {
	uint32 ver_start =
			headerString.begins("NetImmerse File Format")	? 32
		:	headerString.begins("Gamebryo File Format")		? 30
		:	headerString.begins("NDSNIF")					? 30
		:	0;
	return ver_start ? ParseVersionString(headerString + ver_start) : VER_INVALID;
}

struct Header {
	HeaderString			headerString;
	array<3, LineString>	copyright;
	uint32					version;
	EndianType				endianType;
	uint32					userVersion;
	mutable uint32			numBlocks;
	uint32					userVersion2;
	uint32					unknownInt3;
	ExportInfo				exportInfo;
	mutable uint16			numBlockTypes;
	dynamic_array<string>	blockTypes;
	dynamic_array<uint16 >	blockTypeIndex;
	dynamic_array<uint32 >	blockSize;
	mutable uint32			numStrings;
	uint32					maxStringLength;
	dynamic_array<string >	strings;
	uint32					unknownInt2;

	NifInfo Read(istream_ref in) {
		auto	text = make_text_reader(in);
		NifInfo info;
		text.read_line(headerString);
		info.version = GetVersion(headerString);

		if (info.version <= 0x03010000) {
			for (uint32 i2 = 0; i2 < 3; i2++)
				text.read_line(copyright[i2]);
		}
		NifStream	nif(in, info);
		if (info.version >= 0x0303000D)
			nif.get(version);
		if (info.version >= 0x14000004)
			nif.get(endianType);
		if (info.version >= 0x0A010000)
			nif.get(userVersion);
		if (info.version >= 0x0303000D)
			nif.get(numBlocks);
		if (info.version >= 0x0A010000) {
			if (userVersion >= 10 || (userVersion == 1 && version != 0x0A020000))
				nif.get(userVersion2);
		}
		if (info.version >= 0x1E000002)
			nif.get(unknownInt3);

		if (info.version >= 0x0A000102 && info.version <= 0x0A000102) {
			if (info.version <= 0x0A000102)
				nif.get(exportInfo.unknown);
			nif.get(exportInfo.creator);
			nif.get(exportInfo.exportInfo1);
			nif.get(exportInfo.exportInfo2);
		}
		if (info.version >= 0x0A010000) {
			if (userVersion >= 10 || (userVersion == 1 && version != 0x0A020000)) {
				if (info.version <= 0x0A000102)
					nif.get(exportInfo.unknown);
				nif.get(exportInfo.creator);
				nif.get(exportInfo.exportInfo1);
				nif.get(exportInfo.exportInfo2);
			}
		}
		if (info.version >= 0x0A000100) {
			nif.get(numBlockTypes);
			blockTypes.resize(numBlockTypes);
			for (uint32 i2 = 0; i2 < blockTypes.size(); i2++)
				nif.get(blockTypes[i2]);
			blockTypeIndex.resize(numBlocks);
			for (uint32 i2 = 0; i2 < blockTypeIndex.size(); i2++)
				nif.get(blockTypeIndex[i2]);
		}
		if (info.version >= 0x14020007) {
			blockSize.resize(numBlocks);
			for (uint32 i2 = 0; i2 < blockSize.size(); i2++)
				nif.get(blockSize[i2]);
		}
		if (info.version >= 0x14010003) {
			nif.get(numStrings);
			nif.get(maxStringLength);
			strings.resize(numStrings);
			for (uint32 i2 = 0; i2 < strings.size(); i2++)
				nif.get(strings[i2]);
		}
		if (info.version >= 0x0A000100)
			nif.get(unknownInt2);

		//Copy info.version to local version var.
		version = info.version;

		//Fill out and return NifInfo structure.
		info.userVersion	= userVersion;
		info.userVersion2	= userVersion2;
		info.endian			= endianType;
		info.creator		= exportInfo.creator;
		info.exportInfo1	= exportInfo.exportInfo1;
		info.exportInfo2	= exportInfo.exportInfo2;

		return info;
	}
};

struct Footer {
	mutable uint32 numRoots;
	dynamic_array<Ref<NiObject > > roots;

	Footer() : numRoots(0) {}
	void Read(istream_ref in, list<uint32> &link_stack, const NifInfo &info) {
		NifStream	nif(in, info);
		if (info.version >= 0x0303000D) {
			nif.get(numRoots);
			roots.resize(numRoots);
			for (unsigned int i2 = 0; i2 < roots.size(); i2++)
				link_stack.push_back(nif.get<uint32>());
		}
	}
};

list<Ref<NiObject> >	ResolveMissingLinkStack(NiObject *root, const list<NiObject*> & missing_link_stack);
Ref<NiNode>				FindCommonAncestor(const dynamic_array<Ref<NiAVObject> > &objects);
list< Ref<NiNode> >		ListAncestors(NiAVObject *leaf);
bool					IsSupportedVersion(uint32 version);

void RegisterObjects() {
	static bool done = false;
	done = true;
}

dynamic_array<NiObjectRef> ReadNifList(istream_ref in, list<NiObjectRef> &missing_link_stack, NifInfo *info) {
	//Ensure that objects are registered
	RegisterObjects();

	//--Read Header--
	Header header;

	//Create a new NifInfo if one isn't given.
	bool delete_info = !info;
	if (!info)
		info = new NifInfo();

	//Read header.
	*info = header.Read(in);

	//--Read Objects--
	size_t						numObjects = header.numBlocks;
	map<uint32, NiObjectRef>	objects; //Map to hold objects by number
	dynamic_array<NiObjectRef>	obj_list; //Vector to hold links in the order they were created.
	list<uint32>				link_stack; //List to add link values to as they're read in from the file

	streamptr nextobjpos	= in.tell();

	//Loop through all objects in the file
	for (uint32 i = 0; header.version < VER_3_3_0_13 || i < numObjects; ++i) {
		// Check if the size information matches in version 20.3 and greater
		if (header.version >= VER_20_3_0_3) {
			in.seek(nextobjpos);
			nextobjpos += header.blockSize[i];
		}

		string		objectType;

		//There are two main ways to read objects - one before version 5.0.0.1 and one after
		if (header.version >= 0x05000001) {
			//From version 5.0.0.1 to version 10.0.1.106  there is a zero byte at the begining of each object
			if (header.version <= VER_10_1_0_106)
				return obj_list;

			// Find which NIF object type this is by using the header arrays
			objectType = header.blockTypes[header.blockTypeIndex[i]];
		} else {
			// Find which object type this is by reading the string at this location
			uint32 len = in.get<uint32>();
			if (len > 30 || len < 6)
				return obj_list;

			objectType.resize(len);
			in.readbuff(objectType.begin(), len);

			if (header.version < VER_3_3_0_13) {
				//There can be special commands instead of object names in these versions
				if (objectType == "Top Level Object")
					continue;
				if (objectType == "End Of File")
					break;
			}
		}

		//Create object of the type that was found
		NiObjectRef new_obj = ObjectRegistry::CreateObject(objectType);

		//Check for an unknown object type
		if (!new_obj)
			return obj_list;

		//Read new object
		new_obj->Read(in, link_stack, *info);

		//Add object to map
		uint32 index					= header.version < VER_3_3_0_13 ? in.get<uint32>() : i;
		new_obj->internal_block_number	= index;
		objects[index]					= new_obj;

		//Add object to list
		obj_list.push_back(new_obj);
	}

	//--Read Footer--//
	Footer footer;
	footer.Read(in, link_stack, *info);

	//--Now that all objects are read, go back and fix the links--//
	for (uint32 i = 0; i < obj_list.size(); ++i)
		obj_list[i]->FixLinks(objects, link_stack, missing_link_stack, *info);

	//delete info if it was dynamically allocated
	if (delete_info)
		delete info;

	//Return completed object list
	return obj_list;
}

NiObjectRef FindRoot(dynamic_array<NiObjectRef> const & objects) {
	//--Look for a NiNode that has no parents--//
	//Find the first NiObjectNET derived object
	NiAVObjectRef root;
	for (uint32 i = 0; i < objects.size(); ++i) {
		root = DynamicCast<NiAVObject>(objects[i]);
		if (root)
			break;
	}

	//Make sure a node was found, if not return first node
	if (root == NULL)
		return objects[0];

	//Move up the chain to the root node
	while (root->GetParent())
		root = StaticCast<NiAVObject>(root->GetParent());
	return StaticCast<NiObject>(root);
}

NiObject *_ResolveMissingLinkStackHelper(NiObject *root, NiObject *obj) {
	// search by name
	NiNodeRef rootnode	= DynamicCast<NiNode>(root);
	NiNodeRef objnode	= DynamicCast<NiNode>(obj);

	if (rootnode && objnode) {
		if (rootnode->GetName() && rootnode->GetName() == objnode->GetName())
			return rootnode;

		list<NiObjectRef> children = root->GetRefs();
		for (list<NiObjectRef>::iterator child = children.begin(); child != children.end(); ++child) {
			if (NiObject *result = _ResolveMissingLinkStackHelper(*child, obj))
				return result;
		}
	}
	// nothing found
	return 0;
}

list<NiObjectRef> ResolveMissingLinkStack(NiObject *root, const list<NiObject*> &missing_link_stack) {
	list<NiObjectRef> result;
	for (list<NiObject *>::const_iterator obj = missing_link_stack.begin(); obj != missing_link_stack.end(); ++obj)
		result.push_back(_ResolveMissingLinkStackHelper(root, *obj));
	return result;
}


// Determine whether block comes before its parent or not, depending on the block type.
// return: 'True' if child should come first, 'False' otherwise.
bool BlockChildBeforeParent(NiObject *root) {
	const Type &t = root->GetType();
	return t.IsDerivedType(bhkRefObject::TYPE) && !t.IsDerivedType(bhkConstraint::TYPE);
}

bool IsSupportedVersion(uint32 version) {
	switch (version) {
		case VER_2_3:
		case VER_3_0:
		case VER_3_03:
		case VER_3_1:
		case VER_3_3_0_13:
		case VER_4_0_0_0:
		case VER_4_0_0_2:
		case VER_4_1_0_12:
		case VER_4_2_0_2:
		case VER_4_2_1_0:
		case VER_4_2_2_0:
		case VER_10_0_1_0:
		case VER_10_0_1_2:
		case VER_10_0_1_3:
		case VER_10_1_0_0:
		case VER_10_1_0_101:
		case VER_10_1_0_106:
		case VER_10_2_0_0:
		case VER_10_4_0_1:
		case VER_20_0_0_4:
		case VER_20_0_0_5:
		case VER_20_1_0_3:
		case VER_20_2_0_7:
		case VER_20_2_0_8:
		case VER_20_3_0_1:
		case VER_20_3_0_2:
		case VER_20_3_0_3:
		case VER_20_3_0_6:
		case VER_20_3_0_9:
			return true;
	}
	return false;
}

list<Ref<NiNode> > ListAncestors(NiAVObject *leaf) {
	list<NiNodeRef> ancestors;
	for (NiNodeRef current = leaf->GetParent(); current; current = current->GetParent())
		ancestors.push_front(current);
	return ancestors;
}

Ref<NiNode> FindCommonAncestor(const dynamic_array<Ref<NiAVObject> > &objects) {
	//create lists of nodes that have an influence and this TriBasedGeomas decendents
	size_t obj_count = objects.size();
	dynamic_array<list<NiNodeRef> > ancestors(obj_count);

	//Add Ancestors of each object to its corresponding list
	for (size_t i = 0; i < obj_count; ++i)
		ancestors[i] = ListAncestors(objects[i]);

	if (ancestors[0].size() == 0) {
		//All objects must have a parent for there to be a common ancestor, so return NULL
		return NULL;
	}

	NiNodeRef root = ancestors[0].front();
	//Make sure bone and shapes are part of the same tree
	for (size_t i = 1; i < obj_count; ++i) {
		if (ancestors[i].size() == 0) {
			//All objects must have a parent for there to be a common ancestor, so return NULL
			return NULL;
		}
		if (ancestors[i].front() != root) {
			//These objects are not part of the same tree, so return NULL
			return NULL;
		}
	}

	//Since the first items have been shown to match, pop all the stacks
	for (size_t i = 0; i < obj_count; ++i)
		ancestors[i].pop_front();

	//Now search for the common ancestor
	for (;;) {
		bool all_same = true;
		if (ancestors[0].size() == 0) {
			//This list is over, so the last top is the common ancestor
			//break out of the loop
			break;
		}
		NiNodeRef first_ancestor = ancestors[0].front();
		for (size_t i = 1; i < obj_count; ++i) {
			if (ancestors[i].size() == 0) {
				//This list is over, so the last top is the common ancestor break out of the loop
				all_same = false;
				break;
			}
			if (ancestors[i].front() != first_ancestor)
				all_same = false;
		}

		if (all_same == true) {
			//They're all the same, so set the top, pop all the stacks and look again
			root = ancestors[0].front();
			for (size_t i = 0; i < obj_count; ++i)
				ancestors[i].pop_front();
		} else {
			//One is different, so the last top is the common ancestor - break out of the loop
			break;
		}
	}

	return root;
}

}//namespace nif

using namespace nif;

class NIFFileHandler : public FileHandler {
	const char*		GetExt() override { return "nif"; }
	const char*		GetDescription() override { return "GameBryo file"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		string		header;
		return make_text_reader(file).read_line(header) && IsSupportedVersion(GetVersion(header)) ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		NifInfo				*info	= 0;
		list<NiObjectRef>	missing_link_stack;
		auto a = ReadNifList(file, missing_link_stack, info);
		return ISO_NULL;
	}
} nif_fh;