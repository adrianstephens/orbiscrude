#include "iso/iso_files.h"
#include "base/vector.h"
#include "extra/random.h"
#include "vector_iso.h"
#include "container/archive_help.h"

#include "codec/snappy.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	basic types
//-----------------------------------------------------------------------------

// the image formats that we support
#define WVR_IMAGE_FORMAT_UNDEFINED	0
#define WVR_IMAGE_FORMAT_RGB	1
#define WVR_IMAGE_FORMAT_RGBA	2

enum WVR_Status {
	wvrSuccess = 0,								//!< All's well.
	wvrErrorMemoryAllocate = -1,				//!< Could not allocate memory.
	wvrErrorInvalidData = -2,					//!< Invalid input parameters to function.
	wvrBusy = -3,								//!< Function could not be re-entered.
	wvrFileOpenFailed = -4,						//!< The file failed to open.
	wvrFileWriteFailed = -5,					//!< The file failed to write an item.
	wvrFileWriteBufferFailed = -6,				//!< The file failed to write an item.
	wvrFileReadInvalidFile = -7,				//!< The magic number for the file is invalid.
	wvrFileReadNewVersion = -8,					//!< The file is a newer version than we support.
	wvrFileReadFailed = -9,						//!< A read operation failed to return data.
	wvrFileReadUnresolved = -10,				//!< Unresolved item reference on read.
	wvrFileSeekFailed = -11,					//!< Failed to move the File Descriptor by required bytes.
	wvrFileScriptFrameworkMissing = -12,		//!< Script framework missing from WVR file.
	wvrFileReadEntityVersionUnsupported = -13,	//!< The file contains an entity with an unsupported version.
	wvrReaderUnknownEntity = -100,				//!< Encountered an object we did not understand.
	wvrWriterInvalidState = -101,				//!< The writer is completely fucked up.
	wvrInvalidEntityName = -102,				//!< Invalid name; it already exists, or has bad characters in it.
	wvrUnimplementedHardware = -103,			//!< Hardware/OS combination is not implemented for specified command.
	wvrLibUninitialized = -104,					//!< The WVRLib has not been initialized (or has been shutdown)
	wvrAudioDriverInit = -200,					//!< Audio device could not be opened
	wvrFailValidateSceneCount = -300,			//!< Validation failed: Scene count is 0.
	wvrFailValidateMovieFile = -301,			//!< Validation failed: Invalid movie file.
	wvrFailValidateImageFile = -302,			//!< Validation failed: Invalid image file.
	wvrFailValidateHotspotKey = -303,			//!< Validation failed: Invalid hotspot key.
	wvrFailValidateSceneNoChild = -304,			//!< Validation failed: Scene contains no children.
	wvrValidationFailed = -399,					//!< Validation failed: Appropriate error thrown.
	wvrFailRunInvalidScene = -400,				//!< Could not run because the default scene is invalid.
	wvrEngineUninitialized = -500,				//!< The engine is uninitialzied; all calls are invalid.
	wvrEngineAlreadyInitialized = -501,			//!< The engine is already initialized; something's rotten.
	wvrLuaStateAlloc = -600,					//!< The engine could not allocate a new lua script.
	wvrLuaError = -601,							//!< LUA returned an error.
	wvrAndroidCreateVM = -700,					//!< Could not create Java VM.
	wvrAndroidGetEnv = -701,					//!< Could not get Java Environment.
	wvrAndroidGetMP = -702,						//!< Could not get Java Movie Player Controller class.
	wvrAndroidGetMethod = -703,					//!< Could not get Java Movie Player Create method.
	wvrMovieOpenFailed = -800,					//!< Could not access the input stream for a movie.
	wvrMovieInvalidStream = -801,				//!< The movie stream data was unavailable or incomprehensible.
	wvrMovieInvalidVideoCodec = -802,			//!< The movie stream is encoded in an unsupported codec.
	wvrMovieLibAVError = -803,					//!< A catchall error for errors coming from libAV.
	wvrAudioTrackOutOfRange = -804,				//!< The Audio Track index is out of range.
	wvrAudioInvalidChannelCount = -805,			//!< The channel count is not a supported value.
	wvrMovieVideoDecodeFailed = -806,			//!< The video movie stream failed to decode mid-stream.
	wvrMovieInvalidAudioCodec = -807,			//!< The audio stream is encoded in an unsupported codec.
	wvrAudioInvalidChannelLayout = -808,		//!< The channel layout is not the supported value.
	wvrAudioInvalidName = -809,
	wvrAudioFileEmpty = -810,
	wvrAudioFileReadFailed = -811,
	wvrSnappyCompressError = -900,				//!< Snappy compression error.
	wvrSnappyUncompressError = -901,			//!< Snappy decompression error.
	wvrSTDError = -1001,						//!< Got an unexpected exception from the STD library.
	wvrControllerInvalidInputOrOutput = -2000,	//!< Controller Read Failed.
	wvrErrorUnknown = -3002,					//!< Something bad happened.
};

enum WVR_TypeID {
	WVR_TYPE_ID_FILE				= 1000,
	WVR_TYPE_ID_SCENE				= 1001,
	WVR_TYPE_ID_SCRIPT				= 1002,
	WVR_TYPE_ID_GROUP				= 1003,
	WVR_TYPE_ID_SWITCHRENDERABLE	= 1004,
	WVR_TYPE_ID_PROJSCREEN			= 1005,
	WVR_TYPE_ID_TRIMESH				= 1006,
	WVR_TYPE_ID_MOVIE				= 1007,
	WVR_TYPE_ID_IMAGE				= 1008,
	WVR_TYPE_ID_AUDIOBANK			= 1009,
	WVR_TYPE_ID_TEXTUREHOTSPOTS		= 1010,
	WVR_TYPE_ID_RENDERABLE			= 9000,		// Intermediate type - not directly accessible
};

struct WVR_String {
	uint32	len;
	embedded_string	s;
};

class WVR_UUID {
	uint8		data[16];
public:
	WVR_UUID() { clear(*this); }
	WVR_UUID&	operator=(const string &s) {
		if (const char	*p = s.begin()) {
			for (int i = 0; i < 16; i++) {
				if (*p == '-')
					++p;
				data[i] = from_digit(p[0]) * 16 + from_digit(p[1]);
				p += 2;
			}
		} else {
			clear(*this);
		}
		return *this;
	}
	bool		operator==(const WVR_UUID &other) const { return memcmp(data, other.data, 16) == 0; }
	WVR_UUID&	setRandom() {
		uint32	*u = (uint32*)this;
		u[0] = iso::random;
		u[1] = iso::random;
		u[2] = iso::random;
		u[3] = iso::random;
		// set variant: must be 0b10xxxxxx
		data[8] = (data[8] & 0xBF) | 0x80;
		// set version: must be 0b0100xxxx
		data[6] = (data[6] & 0x4F) | 0x40;
		return *this;
	}
};

//uint64 hash(const WVR_UUID &id) { return reinterpret_cast<const uint64&>(id); }

struct WVR_Date {
	uint16	year;
	uint8	month;
	uint8	day;
	WVR_Date() { clear(*this); }
};

struct WVR_GeomVertex {
	static const uint16	cCurrentVersion = 1;
	float3p p;
	float3p n;
	float2p t;
};

struct WVR_GeomAABox {
	static const uint16	cCurrentVersion = 1;
	float3p	mini;	//!< minimum point
	float3p	maxi;	//!< maximum point
	uint16	valid;	//!< has this box been set / is it valid

	WVR_GeomAABox() { valid = false; }
	void merge(const WVR_GeomAABox &n) {
		if (valid) {
			mini = min(mini, n.mini);
			maxi = max(maxi, n.maxi);
		} else {
			mini = n.mini;
			maxi = n.maxi;
			valid = true;
		}
	}
};

struct WVR_VertexBuffer {
	static const uint16	cCurrentVersion = 1;
	dynamic_array<WVR_GeomVertex>	vertexList;
};

struct WVR_IndexBuffer {
	static const uint16	cCurrentVersion = 1;
	dynamic_array<uint32>	indexList;
};

struct WVR_GeomObject {
	static const uint16	cCurrentVersion = 1;
	shared_ptr<WVR_VertexBuffer>	vertBuf;
	shared_ptr<WVR_IndexBuffer>		indBuf;
	WVR_GeomAABox	boundingBox;
};

struct WVR_GeomModel {
	static const uint16	cCurrentVersion = 1;
	dynamic_array<shared_ptr<WVR_GeomObject> > objects;
	WVR_GeomAABox	boundingBox;
};

struct WVR_ColorRGBA {
	WVR_ColorRGBA() {}
	WVR_ColorRGBA(uint32 color) : value(color) {}
	WVR_ColorRGBA(uint8 red, uint8 green, uint8 blue, uint8 alpha) : r(red), g(green), b(blue), a(alpha) {}
	union {
		struct {
			uint8 r, g, b, a;
		};
		uint32 value;
	};
};

struct WVR_EventHandler {
	static const uint16	cCurrentVersion = 2;
	string					eventFilter;
	dynamic_array<string>	actionList;
};


class WVR_AudioTrackDescriptor {
	static const uint16       cCurrentVersion = 3;

public:

	WVR_AudioTrackDescriptor() {}

	bool			getIsPositional() const { return persistent.isPositional; }
	float3p			getPosition() const { return persistent.position; }
	float3p			getEmitterDirection() const { return persistent.emitterDirection; }
	float32			getEmitterSpreadInner() const { return persistent.emitterSpreadInner; }
	float32			getEmitterSpreadOuter() const { return persistent.emitterSpreadOuter; }
	float32			getAmbientFraction() const { return persistent.ambientFraction; }
	float32			getVolume() const { return persistent.volume; }
	float32			getMinAttenuateDistance() const { return persistent.minAttenuateDistance; }
	float32			getMaxAttenuateDistance() const { return persistent.maxAttenuateDistance; }
	const string&	getPosNodeName() const { return persistent.posNodeName; }
	uint32			getPosNodeType() const { return persistent.posNodeType; }
	uint32			getFlags() const { return persistent.flags; }

	struct Persistent {
		// All fields below were in version 1
		float3p		position;					//!< Where in space the emitter is, relative to the theoretical viewer.
		float3p		emitterDirection;			//!< Direction the emitter is shooting the sound out in.
		float32		emitterSpreadInner;			//!< Inner Angle, centered on direction, audible [0.0-2pi].
		float32		emitterSpreadOuter;			//!< Outer Angle, centered on direction, audible [0.0-2pi].
		float32		ambientFraction;			//!< Percent audible regardless of direction [0.0-1.0].
		float32		volume;						//!< Volume level of audio track [0.0-1.0].
		uint32		flags;						//!< Attributes of the audio.
		string		posNodeName;				//!< Node to which positional audio track is attached.
		uint32		posNodeType;				//!< Type of the node.
		bool		isPositional;				//!< Flag to check if track is positional.
		// Added in version 2
		float32		minAttenuateDistance;		//!< Near than this, the track will be max volume
		float32		maxAttenuateDistance;		//!< Further than this, the track will be 0 volume.

		Persistent() : emitterSpreadInner(pi * 2), emitterSpreadOuter(pi * 2), ambientFraction(1), volume(1), flags(0), posNodeType(0), minAttenuateDistance(1), maxAttenuateDistance(20), isPositional(false) {
			position = zero;
			emitterDirection= zero;
		}
	} persistent;

	friend class WVR_Writer;
	friend class WVR_Reader;
};

struct WVR_SoundEntry {
	static const uint16			cCurrentVersion = 1;

	WVR_AudioTrackDescriptor    info;
	uint16						startActive;	//!< Should the movie be active immediately on load
	uint16						loop;

	string						fileType;		//!< The original file type that was read in.
	uint64						dataSize;		//!< The size of the source file, 0 if invalid file.
	malloc_block				datav;			//!< data for the sound
};

class WVR_Item;
class WVR_Script;
typedef map<string, string>  WVR_ScriptArguments;

struct WVR_ScriptPrivateLang {
	WVR_ScriptPrivateLang() {
		implementsOnInit = false;
		implementsOnReadFinalized = false;
		implementsOnEvent = false;
		implementsOnFrameBegin = false;
		implementsOnTransitionOut = false;
		implementsOnTransitionIn = false;
		implementsOnTransitionFinished = false;
	}
	virtual ~WVR_ScriptPrivateLang() {};

	bool				implementsOnInit;
	bool				implementsOnReadFinalized;
	bool				implementsOnEvent;
	bool				implementsOnFrameBegin;
	bool				implementsOnTransitionOut;
	bool				implementsOnTransitionIn;
	bool				implementsOnTransitionFinished;

	virtual const char*	getOnInitName() const = 0;
	virtual const char*	getOnReadFinalizedName() const = 0;
	virtual const char*	getOnEventName() const = 0;
	virtual const char*	getOnFrameBeginName() const = 0;
	virtual const char*	getOnTransitionOutName() const = 0;
	virtual const char*	getOnTransitionInName() const = 0;
	virtual const char*	getOnTransitionFinishedName() const = 0;
	virtual void		execute(const char * funcName, void * scriptInstanceId) = 0;
	virtual void		execute(const char * funcName, void * scriptInstanceId, double v) = 0;
};

struct WVR_ScriptInstance {
	static const uint16			cCurrentVersion = 1;

	shared_ptr<WVR_Script>		baseDefinition;
	WVR_ScriptPrivateLang		*theScriptImpl;
	WVR_Item					*boundNode;
	WVR_ScriptArguments         arguments;

	WVR_ScriptInstance(WVR_Item *bn, const WVR_ScriptArguments &args) : boundNode(bn), arguments(args), theScriptImpl(0) {}
	virtual ~WVR_ScriptInstance() {
		delete theScriptImpl;
	}
	/*
	bool				implementsOnInit() const;
	bool				implementsOnReadFinalized() const;
	bool				implementsOnEvent() const;
	bool				implementsOnFrameBegin() const;
	bool				implementsOnTransitionOut() const;
	bool				implementsOnTransitionIn() const;
	bool				implementsOnTransitionFinished() const;
	virtual const char*	getOnInitName() const;
	virtual const char*	getOnReadFinalizedName() const;
	virtual const char*	getOnEventName() const;
	virtual const char*	getOnFrameBeginName() const;
	virtual const char*	getOnTransitionOutName() const;
	virtual const char*	getOnTransitionInName() const;
	virtual const char*	getOnTransitionFinishedName() const;
	void				execute(const char * funcName);
	void				execute(const char * funcName, double v);
	*/
};


//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------
class WVR_VersionedImpl;

class WVR_Item : public ISO::VirtualDefaults {
public:
	virtual						~WVR_Item() {}
	virtual const string &		getName()		const;
	virtual void				setName(const string &n);
	virtual WVR_TypeID			getType()		const;
	virtual WVR_VersionedImpl *	getPimpl()		const = 0;
	virtual bool				isRenderable()	const { return false; }
	virtual bool				isPositional()	const { return false; }
	virtual void				cleanup() {}
	ISO_ptr<void>		Deref() const;
};

class WVR_Renderable : public WVR_Item {
public:
	WVR_Renderable() {}
	virtual ~WVR_Renderable() {}

	const WVR_GeomAABox&		getBoundingBox();
	virtual bool				isRenderable() const { return true; }
	virtual bool				isPositional() const { return true; }
};

class WVR_Geometry : public WVR_Item {
public:
	WVR_Geometry() {}
	const WVR_GeomAABox&		getBoundingBox();
};

enum TextureStereoType {
	eTextureStereoTypeMono = 0,
	eTextureStereoTypeOULR = 1,
	eTextureStereoTypeOURL = 2,
	eTextureStereoTypeSSLR = 3,
	eTextureStereoTypeSSRL = 4,
	eTextureStereoTypeCount = 5,
};

/// Different encoding of alpha into a texture
enum TextureAlphaType {
	eTextureAlphaTypeNone = 0,
	eTextureAlphaTypePixel = 1,
	eTextureAlphaTypeOUCA = 2,
	eTextureAlphaTypeOUAC = 3,
	eTextureAlphaTypeSSCA = 4,
	eTextureAlphaTypeSSAC = 5,
	eTextureAlphaTypeCount = 6,
};

class WVR_Texture : public WVR_Item {
public:
	// abstract methods that all subclasses must implement
	virtual bool				isDynamic()				const = 0;
	virtual uint32				getWidth()				const = 0;
	virtual uint32				getHeight()				const = 0;
	virtual uint32				getPixelFormat()		const = 0;
	virtual uint32				getRawDataStrideBytes() const = 0;
	virtual void *				getRawData()			const = 0;
	virtual TextureStereoType	getStereoType()			const = 0;
	virtual TextureAlphaType	getAlphaType()			const = 0;
};

class WVR_Movie_Impl;
class WVR_Movie : public WVR_Texture {
public:
	WVR_Movie();
	virtual	~WVR_Movie();

	WVR_VersionedImpl*	getPimpl() const override { return (WVR_VersionedImpl*)pImpl; }

	bool				isDynamic()				const override { return true; }
	uint32				getWidth()				const override { return 0; }
	uint32				getHeight()				const override { return 0; }
	uint32				getPixelFormat()		const override { return WVR_IMAGE_FORMAT_UNDEFINED;  }
	uint32				getRawDataStrideBytes() const override { return 0; }
	void *				getRawData()			const override { return 0; }
	TextureStereoType	getStereoType()			const override;
	TextureAlphaType	getAlphaType()			const override;

	WVR_Movie_Impl	*pImpl;
};

class WVR_AudioBank_Impl;
class WVR_AudioBank : public WVR_Item {
public:
	// Public constructors and destructors.
	WVR_AudioBank();
	virtual	~WVR_AudioBank();

	// public methods dictated by the abstract parent class
	WVR_VersionedImpl*	getPimpl() const override { return (WVR_VersionedImpl*)pImpl; }

	// our own functionality
//	virtual uint32				getSoundCount() const;
//	virtual void				start(uint32 index);
//	virtual void				pause(uint32 index);
//	virtual void				resume(uint32 index);

	WVR_AudioBank_Impl	*pImpl;
};

class WVR_Image_Impl;
class WVR_Image : public WVR_Texture {
public:
	// Public constructors and destructors.
	WVR_Image();
	virtual	~WVR_Image();

	// public methods dictated by the abstract parent class
	WVR_VersionedImpl*	getPimpl() const override { return (WVR_VersionedImpl*)pImpl; }

	// abstract methods that all WVR_Texture subclasses must implement
	bool				isDynamic()				const override;
	uint32				getWidth()				const override;
	uint32				getHeight()				const override;
	uint32				getPixelFormat()		const override;
	uint32				getRawDataStrideBytes() const override;
	void *				getRawData()			const override;
	TextureStereoType	getStereoType()			const override;
	TextureAlphaType	getAlphaType()			const override;

	WVR_Image_Impl	*pImpl;
};

typedef pair<string, WVR_ColorRGBA> WVR_HotspotKey;
class WVR_TextureHotspots_Impl;
class WVR_TextureHotspots : public WVR_Texture {
public:
	// Public constructors and destructors.
	WVR_TextureHotspots();
	virtual ~WVR_TextureHotspots();

	// public methods dictated by the abstract parent class
	WVR_VersionedImpl*	getPimpl() const override { return (WVR_VersionedImpl*)pImpl; }

	bool				isDynamic()				const override;
	uint32				getWidth()				const override;
	uint32				getHeight()				const override;
	uint32				getPixelFormat()		const override;
	uint32				getRawDataStrideBytes() const override;
	void *				getRawData()			const override;
	TextureStereoType	getStereoType()			const override;
	TextureAlphaType	getAlphaType()			const override;

	WVR_Status			setDisplaySource(WVR_Texture*);
	WVR_Texture*		getDisplaySource() const;
	WVR_Status			setHotspotSource(WVR_Texture*);
	WVR_Texture*		getHotspotSource() const;
	WVR_Status			addHotspotKey(const WVR_HotspotKey&);
	const dynamic_array<WVR_HotspotKey>&	getHotspotKeys() const;

	void				cleanup() override;

	WVR_TextureHotspots_Impl* pImpl;
};

class WVR_TriMesh_Impl;
class WVR_TriMesh : public WVR_Geometry {
public:
	// Public constructors and destructors.
	WVR_TriMesh();
	virtual	~WVR_TriMesh();

	// public methods dictated by the abstract parent class
	WVR_VersionedImpl*	getPimpl() const override { return (WVR_VersionedImpl*)pImpl; }

	// public methods specific to this class
	const WVR_GeomModel&		getModel() const;
	const bool					getCollisionIgnored() const;

	WVR_TriMesh_Impl	*pImpl;
};

class WVR_ProjScreen_Impl;
class WVR_ProjScreen : public WVR_Renderable {
public:
	// Public constructors and destructors.
	WVR_ProjScreen();
	virtual	~WVR_ProjScreen();

	// public methods dictated by the abstract parent class
	WVR_VersionedImpl*	getPimpl() const override { return (WVR_VersionedImpl*)pImpl; }

	// public methods specific to this class
	WVR_Status		setGeometry(WVR_TriMesh*);
	WVR_TriMesh*	getGeometry() const;
	WVR_Status		setTexture(WVR_Texture*);
	WVR_Texture*	getTexture() const;

	void			cleanup() override;
	WVR_ProjScreen_Impl	*pImpl;
};

class WVR_Group_Impl;
class WVR_Group : public WVR_Renderable {
public:
	// Constructors & copy operators
	WVR_Group();
	virtual	~WVR_Group();

	// public methods dictated by the abstract parent class
	WVR_VersionedImpl*	getPimpl() const override { return (WVR_VersionedImpl*)pImpl; }

	// methods special to this class
	uint32			getChildCount() const;
	WVR_Renderable*	getChild(uint32) const;

	void			cleanup() override;

	WVR_Group_Impl	*pImpl;
};

class WVR_SwitchRenderable_Impl;
class WVR_SwitchRenderable : public WVR_Renderable {
public:
	// Constructors & copy operators
	WVR_SwitchRenderable();
	virtual	~WVR_SwitchRenderable();

	// public methods dictated by the abstract parent class
	WVR_VersionedImpl*	getPimpl() const override { return (WVR_VersionedImpl*)pImpl; }

	// methods special to this class
	uint32			getChildCount() const;
	WVR_Renderable*	getChild(uint32) const;
	void			setCurrentChild(uint32);
	uint32			getCurrentChild() const;

	void			cleanup() override;

	WVR_SwitchRenderable_Impl	*pImpl;
};

class WVR_Script_Impl;
class WVR_Script : public WVR_Item {
public:
	// Public constructors and destructors.
	WVR_Script();
	virtual	~WVR_Script();

	// public methods dictated by the abstract parent class
	WVR_VersionedImpl*	getPimpl() const override { return (WVR_VersionedImpl*)pImpl; }

	//public methods special to us
	virtual WVR_Status			setScriptByteCode(const string &);
	WVR_Script_Impl	*pImpl;
};

class WVR_Scene_Impl;
class WVR_Scene : public WVR_Renderable {
public:
	// Constructors & copy operators
	WVR_Scene();
	virtual	~WVR_Scene();

	// public methods dictated by the abstract parent class
	WVR_VersionedImpl*	getPimpl() const override { return (WVR_VersionedImpl*)pImpl; }

	// public methods for adding/accessing items to the scene
	WVR_Renderable*	getRoot();
	void			setRoot(WVR_Renderable*);
	void			setAudioBank(WVR_AudioBank*);

	//public methods to control the scene
	WVR_Status		playSound(uint32 indexInAudioBank);

	void			cleanup() override;
	WVR_Scene_Impl	*pImpl;
};

//-----------------------------------------------------------------------------
//	Impls
//-----------------------------------------------------------------------------

class WVR_VersionedImpl {
public:
	void			setName(const string &n)	{ instanceName = n; }
	const string&	getName()		const		{ return instanceName; };
	void			setId(WVR_UUID newId)		{ instanceId = newId; }
	WVR_UUID		getId()			const		{ return instanceId; }
	WVR_TypeID		getType()		const		{ return WVR_TypeID(itemType); }

	// abstract functions that all subclasses must implement
	virtual dynamic_array<shared_ptr<WVR_Item> >	getDescendants() { return dynamic_array<shared_ptr<WVR_Item> >(); }

	// static members
	static uint16	getCurrentBaseVersion()			{ return cCurrentBaseVersion; };
	static void		addChildAndDescendants(dynamic_array<shared_ptr<WVR_Item> > &ret, WVR_Item *x) {
		dynamic_array<shared_ptr<WVR_Item> > fromChild = x->getPimpl()->getDescendants();
		ret.insert(ret.end(), fromChild.begin(), fromChild.end());
		ret.push_back(x);
	}
	// member methods for subclasses to override
	virtual bool	isRenderable()		const	{ return false; }
	virtual dynamic_array<WVR_EventHandler> &getEventHandlerList() final { return eventHandlers;  }

	void			assignScript(WVR_Item *n, WVR_Script *scr, const WVR_ScriptArguments &args) {
		assignedScript = new WVR_ScriptInstance(n, args);
		assignedScript->baseDefinition = scr;
	}
	WVR_ScriptInstance*		getAssignedScript()		const	{ return assignedScript; }

	static const uint16	cCurrentBaseVersion = 1;

	// only subclasses should be creating and destroying
	WVR_VersionedImpl(uint16 subclassType, uint16 subclassVersion) {
		itemType	= subclassType;
		baseVersion	= cCurrentBaseVersion;
		version		= subclassVersion;
		instanceId	= WVR_UUID();
	}
	virtual ~WVR_VersionedImpl() {}

	// member methods for subclasses to override
	virtual void	oneTimePrepare() {}

public:
	uint16			itemType;		//!< Uniquely dentifies the type of the item.
	uint16			baseVersion;	//!< Version number for the base WVR_Item class.
	uint16			version;		//!< Version number for the derived class.

	string			instanceName;	//!< Human readable name for the instance.
	WVR_UUID		instanceId;		//!< Id that uniquely identifies each WVR_Item in this WVR_File.

	shared_ptr<WVR_ScriptInstance>	assignedScript;
	dynamic_array<WVR_EventHandler>	eventHandlers;
};

const string&	WVR_Item::getName() const			{ return getPimpl()->getName(); }
void			WVR_Item::setName(const string &n)	{ getPimpl()->setName(n); }
WVR_TypeID		WVR_Item::getType() const			{ return getPimpl()->getType(); }

struct WVR_Transform {
    static const uint16      cCurrentVersion = 1;

    float3		position;
    quaternion	rotation;
    float3		scale;
	WVR_Transform() : position(zero), rotation(identity), scale(one) {}
};

class WVR_Renderable_Impl : public WVR_VersionedImpl {
public:
	static const uint16	cCurrentVersion = 2;

	WVR_Renderable_Impl(uint16 subclassType, uint16 subclassVersion) : WVR_VersionedImpl(subclassType, subclassVersion) {
		persistent.flags = 0;
	}

	// overridden from the parent
	virtual bool isRenderable() const final { return true; }

	// abstract methods that all subclasses of Renderable must implement
	virtual void recomputeBoundingBox() {};
	const WVR_GeomAABox &getBoundingBox() {
		if (!persistent.boundingBox.valid)
			recomputeBoundingBox();
		return persistent.boundingBox;
	}

	// the persistent data that is saved in a file
	struct Persistent {
		WVR_Transform   startTransform;
		WVR_GeomAABox	boundingBox;
		uint32			flags;	// added in version 2
	} persistent;
};
const WVR_GeomAABox&	WVR_Renderable::getBoundingBox()				{ return static_cast<WVR_Renderable_Impl*>(getPimpl())->getBoundingBox();}

class WVR_Scene_Impl : public WVR_Renderable_Impl {
public:
	static const uint16	cCurrentVersion = 2;

	// constructors & destructors
	WVR_Scene_Impl() : WVR_Renderable_Impl(WVR_TYPE_ID_SCENE, cCurrentVersion) {}

	// runtime calls for getting/setting the attributes
    WVR_Renderable*		getSceneRoot() const { return persistent.rootNode; }
    WVR_AudioBank*		getAudioBank() const { return persistent.audioBank; }
    void				setSceneRoot(WVR_Renderable *newRoot) { persistent.rootNode = newRoot; }
    void				setAudioBank(WVR_AudioBank *newBank) { persistent.audioBank = newBank; }
    void				setVisible(bool);

	void doTraverseSetMoviesActive(WVR_Renderable* item, bool newActiveState);
	void doTraverseMovies(WVR_Renderable* item, function<void(WVR_Movie*)> action);
	void recomputeBoundingBox() {
		WVR_GeomAABox &theBox = WVR_Renderable_Impl::persistent.boundingBox;
		if (persistent.rootNode)
			theBox = persistent.rootNode->getBoundingBox();
		else
			theBox.valid = false;
	}

	// objects saved in the file
	struct Persistent {
		shared_ptr<WVR_Renderable>	rootNode;
		shared_ptr<WVR_AudioBank>	audioBank;	// added in version 2
	} persistent;
};

WVR_Scene::WVR_Scene() : pImpl(new WVR_Scene_Impl){}
WVR_Scene::~WVR_Scene() { delete pImpl; }
WVR_Renderable* WVR_Scene::getRoot()						{ return pImpl->getSceneRoot();}
void		WVR_Scene::setRoot(WVR_Renderable *p)			{ pImpl->setSceneRoot(p);}
void		WVR_Scene::setAudioBank(WVR_AudioBank *p)		{ pImpl->setAudioBank(p);}
//WVR_Status WVR_Scene::playSound(uint32 indexInAudioBank)	{ return pImpl->playSound(indexInAudioBank);}
void		WVR_Scene::cleanup()							{ pImpl->setSceneRoot(0);}

class WVR_Script_Impl : public WVR_VersionedImpl {
public:
	static const uint16	cCurrentVersion = 2;

	WVR_Script_Impl() : WVR_VersionedImpl(WVR_TYPE_ID_SCRIPT, cCurrentVersion) {
		persistent.scriptSource = "{ Uninitialized Script }";
	}

	// the persistent data that is saved in a file
	struct Persistent {
		string	scriptSource;
		bool	amActionScript;
	} persistent;
};
WVR_Script::WVR_Script() : pImpl(new WVR_Script_Impl) {}
WVR_Script::~WVR_Script() { delete pImpl; }
WVR_Status WVR_Script::setScriptByteCode(const string &src) {
	pImpl->persistent.scriptSource = src;
	return wvrSuccess;
}

class WVR_Group_Impl : public WVR_Renderable_Impl {
public:
	static const uint16	cCurrentVersion = 1;

	// constructors & destructors
	WVR_Group_Impl() : WVR_Renderable_Impl(WVR_TYPE_ID_GROUP, cCurrentVersion) {}

	// methods dictated by the abstract parent class
	dynamic_array<shared_ptr<WVR_Item> >	getDescendants() override {
	    dynamic_array<shared_ptr<WVR_Item> > ret;
		for (const auto &x : persistent.children)
			addChildAndDescendants(ret, x);
		return ret;
	}
	void recomputeBoundingBox() override {
		WVR_GeomAABox &theBox = WVR_Renderable_Impl::persistent.boundingBox;
		if (persistent.children.size() == 0) {
			theBox.mini = zero;
			theBox.maxi = zero;
			theBox.valid = true;
		}  else  {
			theBox.valid = false;
			for (const auto &x : persistent.children)
				theBox.merge(x->getBoundingBox());
			theBox.valid = true;
		}
	}

	void addChild(WVR_Renderable *c) {
		persistent.children.push_back(c);
	}
	// objects saved in the file
	struct Persistent {
		dynamic_array<shared_ptr<WVR_Renderable>>	children;
	} persistent;
};

WVR_Group::WVR_Group() : pImpl(new WVR_Group_Impl){}
WVR_Group::~WVR_Group()	{ delete pImpl;  }
void			WVR_Group::cleanup()					{ pImpl->persistent.children.clear();}
uint32			WVR_Group::getChildCount()	const	{ return (uint32)pImpl->persistent.children.size(); }
WVR_Renderable *WVR_Group::getChild(uint32 i)	const	{ return pImpl->persistent.children[i];}

class WVR_SwitchRenderable_Impl : public WVR_Renderable_Impl {
public:
	static const uint16	cCurrentVersion = 1;

	// constructors & destructors
	WVR_SwitchRenderable_Impl() : WVR_Renderable_Impl(WVR_TYPE_ID_SWITCHRENDERABLE, cCurrentVersion) {}
	~WVR_SwitchRenderable_Impl() override {}

	// methods dictated by the abstract parent class
	dynamic_array<shared_ptr<WVR_Item> >	getDescendants() override {
		dynamic_array<shared_ptr<WVR_Item> > ret;
		for (const auto &x : persistent.children)
			addChildAndDescendants(ret, x);
		return ret;
	}
	void			addChild(WVR_Renderable *c) {
		persistent.children.push_back(c);
	}

	// the persistent data that is saved in a file
	struct Persistent {
		dynamic_array<shared_ptr<WVR_Renderable>>	children;
	} persistent;
};

WVR_SwitchRenderable::WVR_SwitchRenderable() : pImpl(new WVR_SwitchRenderable_Impl) {}
WVR_SwitchRenderable::~WVR_SwitchRenderable() { delete pImpl; }
void			WVR_SwitchRenderable::cleanup()						{ pImpl->persistent.children.clear();}
uint32			WVR_SwitchRenderable::getChildCount() const		{ return (uint32)pImpl->persistent.children.size();}
WVR_Renderable*	WVR_SwitchRenderable::getChild(uint32 i) const		{ return pImpl->persistent.children[i];}

class WVR_Geometry_Impl : public WVR_VersionedImpl {
public:
	static const uint16      cCurrentVersion = 1;

	WVR_Geometry_Impl(uint16 subclassType, uint16 subclassVersion) : WVR_VersionedImpl(subclassType, subclassVersion) {}

	// abstract methods that all subclasses of Geometry must implement
	virtual const WVR_GeomAABox&					getBoundingBox() = 0;

	// the persistent data that is saved in a file
	struct {
	} persistent;
};
const WVR_GeomAABox&	WVR_Geometry::getBoundingBox() { return static_cast<WVR_Geometry_Impl*>(getPimpl())->getBoundingBox(); }

class WVR_TriMesh_Impl : public WVR_Geometry_Impl {
public:
	static const uint16	cCurrentVersion = 2;

	// bit flags used in persistent.flags
	static const uint32	cFlagCollisionIgnore = 0x00000001;

	WVR_TriMesh_Impl() : WVR_Geometry_Impl(WVR_TYPE_ID_TRIMESH, cCurrentVersion) {  persistent.flags = 0; }

	const WVR_GeomAABox&	getBoundingBox() override {
		return persistent.theModel.boundingBox;
	}

	// the persistent data that is saved in a file
	struct Persistent {
		WVR_GeomModel	theModel;
		uint32			flags;	// added in version 2
	} persistent;

	void addObject(WVR_VertexBuffer *vb, WVR_IndexBuffer *ib) {
		// add the object to the list of objects (there might be others there already)
		WVR_GeomObject	*obj = new WVR_GeomObject;
		obj->vertBuf	= vb;
		obj->indBuf		= ib;
		persistent.theModel.objects.push_back(obj);

		// calculate the bbox of the new object. Invalidate it first, just in case there are no points.
		obj->boundingBox.valid = false;
		bool first = true;
		for (const auto &p : obj->vertBuf->vertexList) {
			WVR_GeomAABox &box = obj->boundingBox;
			if (first) {
				box.mini = box.maxi = p.p;
				box.valid = true;
				first = false;
			}
			else {
				box.mini = min(box.mini, p.p);
				box.maxi = max(box.maxi, p.p);
			}
		}
	}
};
WVR_TriMesh::WVR_TriMesh() : pImpl(new WVR_TriMesh_Impl) {}
WVR_TriMesh::~WVR_TriMesh() { delete pImpl; }
const WVR_GeomModel& WVR_TriMesh::getModel()	const { return pImpl->persistent.theModel;}
const bool WVR_TriMesh::getCollisionIgnored()	const { return (pImpl->persistent.flags & WVR_TriMesh_Impl::cFlagCollisionIgnore); }

class WVR_ProjScreen_Impl : public WVR_Renderable_Impl {
public:
	static const uint16	cCurrentVersion = 1;

	WVR_ProjScreen_Impl() : WVR_Renderable_Impl(WVR_TYPE_ID_PROJSCREEN, cCurrentVersion) {}

	// methods dictated by the abstract parent class
	dynamic_array<shared_ptr<WVR_Item> > getDescendants() override {
		dynamic_array<shared_ptr<WVR_Item> > ret;
	    addChildAndDescendants(ret, persistent.projectionSurface);
		addChildAndDescendants(ret, persistent.projectionTextures);
		return ret;
	}
	void recomputeBoundingBox() override {
		WVR_GeomAABox &theBox = WVR_Renderable_Impl::persistent.boundingBox;
		if (persistent.projectionSurface)
			persistent.projectionSurface->getBoundingBox();
		else
			theBox.valid = false;
	}

	// the persistent data that is saved in a file
	struct Persistent {
		shared_ptr<WVR_Texture>	projectionTextures;
		shared_ptr<WVR_TriMesh>	projectionSurface;
	} persistent;
};
WVR_ProjScreen::WVR_ProjScreen() : pImpl(new WVR_ProjScreen_Impl) {}
WVR_ProjScreen::~WVR_ProjScreen() {	delete pImpl; }

void WVR_ProjScreen::cleanup() {
    pImpl->persistent.projectionTextures = nullptr;
    pImpl->persistent.projectionSurface = nullptr;
}

WVR_Status		WVR_ProjScreen::setGeometry(WVR_TriMesh *g) { pImpl->persistent.projectionSurface = g;    return wvrSuccess;}
WVR_TriMesh*	WVR_ProjScreen::getGeometry() const			{ return pImpl->persistent.projectionSurface;}
WVR_Status		WVR_ProjScreen::setTexture(WVR_Texture *t)	{ pImpl->persistent.projectionTextures = t;    return wvrSuccess;}
WVR_Texture*	WVR_ProjScreen::getTexture() const			{ return pImpl->persistent.projectionTextures;}

class WVR_Movie_Impl : public WVR_VersionedImpl {
public:
	static const uint16	cCurrentVersion = 3;

	WVR_Movie_Impl() : WVR_VersionedImpl(WVR_TYPE_ID_MOVIE, cCurrentVersion) {
		persistent.validData = false;
		persistent.startActive = true;
		persistent.exitOnComplete = false;
		persistent.mp4Size = 0;
		persistent.startingOrientation = 0.5;
		persistent.movieFlags = 0;
		persistent.stereoImageType = eTextureStereoTypeMono;
		persistent.alphaImageType = eTextureAlphaTypeNone;
		persistent.loop = false;
		runtime.mp4OffsetInFile = 0;
		runtime.theMovieId = 0;
	}
	~WVR_Movie_Impl() override {
		//release();
	}

	// the persistent data that is saved in a file
	struct Persistent {
		bool				validData;				//!< Were all data checks successful
		bool				startActive;			//!< Should the movie be active immediately on load
		bool				exitOnComplete;			//!<
		uint64				mp4Size;				//!< The size of the MP4 file, 0 if invalid file.
		float32				startingOrientation;	//!< The starting orientation [0.0 - 1.0] of the view.
		uint32				movieFlags;				//!< Attributes of the movie.
		TextureStereoType	stereoImageType;
		TextureAlphaType	alphaImageType;
		dynamic_array<WVR_AudioTrackDescriptor> audioTracks;	//!< Details of each audio tracks included with the video.
		uint16				loop;
	} persistent;

	// the runtime data that is not saved in a file but represents the current state
	struct {
		uint64				mp4OffsetInFile;		//!< The location of the embedded MP4 in the WVR file.
		string				mp4Name;				//!< The name of the MP4 file.
		uint32				theMovieId;				//!< The movie id in the MovieManager. 0 if not valid.
		string				streamingSourceName;
	} runtime;

	malloc_block	mp4Data;
private:
	virtual void	doAllocateAndStartMoviePlayer() {}
};

WVR_Movie::WVR_Movie() : pImpl(new WVR_Movie_Impl) {}
WVR_Movie::~WVR_Movie() { delete pImpl; }
TextureStereoType	WVR_Movie::getStereoType() const { return pImpl->persistent.stereoImageType; }
TextureAlphaType	WVR_Movie::getAlphaType() const { return pImpl->persistent.alphaImageType; }

class WVR_Image_Impl : public WVR_VersionedImpl {
public:
	static const uint16	cCurrentVersion = 2;

	WVR_Image_Impl() : WVR_VersionedImpl(WVR_TYPE_ID_IMAGE, cCurrentVersion) {
		persistent.pixelFormat = WVR_IMAGE_FORMAT_UNDEFINED;
		persistent.width = 0;
		persistent.height = 0;
		persistent.strideBytes = 0;
		persistent.dataBytes = 0;
		persistent.imageData = nullptr;
		persistent.stereoImageType = eTextureStereoTypeMono;
		persistent.alphaImageType = eTextureAlphaTypeNone;
	}
	~WVR_Image_Impl() override {
		if (persistent.imageData)
			delete[] (uint64*)persistent.imageData;
    }
	// the persistent data that is saved in a file
	struct Persistent {
		uint32				pixelFormat;
		uint32				width;
		uint32				height;
		uint32				strideBytes;
		uint64				dataBytes;
		void *				imageData;
		TextureStereoType	stereoImageType;
		TextureAlphaType	alphaImageType;
	} persistent;
};
WVR_Image::WVR_Image() : pImpl(new WVR_Image_Impl) {}
WVR_Image::~WVR_Image() { delete pImpl; }
bool	WVR_Image::isDynamic()			const { return false;}
uint32	WVR_Image::getWidth()			const { return pImpl->persistent.width;}
uint32	WVR_Image::getHeight()			const { return pImpl->persistent.height;}
uint32	WVR_Image::getPixelFormat()			const { return pImpl->persistent.pixelFormat;}
uint32	WVR_Image::getRawDataStrideBytes()	const { return pImpl->persistent.strideBytes;}
void*	WVR_Image::getRawData()				const { return pImpl->persistent.imageData;}
TextureStereoType WVR_Image::getStereoType()const { return pImpl->persistent.stereoImageType;}
TextureAlphaType WVR_Image::getAlphaType()	const { return pImpl->persistent.alphaImageType;}

class WVR_AudioBank_Impl : public WVR_VersionedImpl {
public:
	static const uint16	cCurrentVersion = 1;

	WVR_AudioBank_Impl() : WVR_VersionedImpl(WVR_TYPE_ID_AUDIOBANK, cCurrentVersion) {}

	// protected methods overidden from the parent class
	virtual void	oneTimePrepare() {}

	// the persistent data that is saved in a file
	struct Persistent {
		dynamic_array<WVR_SoundEntry> audioTracks;	//!< Details of each audio tracks included with the video.
	} persistent;
};

WVR_AudioBank::WVR_AudioBank() : pImpl(new WVR_AudioBank_Impl) {}
WVR_AudioBank::~WVR_AudioBank() { delete pImpl; }

class WVR_TextureHotspots_Impl : public WVR_VersionedImpl {
public:
	static const uint16	cCurrentVersion = 1;

	WVR_TextureHotspots_Impl() : WVR_VersionedImpl(WVR_TYPE_ID_TEXTUREHOTSPOTS, cCurrentVersion) {}

	struct Persistent {
		shared_ptr<WVR_Texture>	displaySource;
		shared_ptr<WVR_Texture>	hotspotSource;
		dynamic_array<pair<string, WVR_ColorRGBA> >	hotspotKeys;
	} persistent;
};

WVR_TextureHotspots::WVR_TextureHotspots() : pImpl(new WVR_TextureHotspots_Impl) {}
WVR_TextureHotspots::~WVR_TextureHotspots() { delete pImpl; }
bool				WVR_TextureHotspots::isDynamic()						const	{ return getDisplaySource()->isDynamic();}
uint32				WVR_TextureHotspots::getWidth()							const	{ return getDisplaySource()->getWidth();}
uint32				WVR_TextureHotspots::getHeight()						const	{ return getDisplaySource()->getHeight();}
uint32				WVR_TextureHotspots::getPixelFormat()					const	{ return getDisplaySource()->getPixelFormat();}
uint32				WVR_TextureHotspots::getRawDataStrideBytes()			const	{ return getDisplaySource()->getRawDataStrideBytes();}
void*				WVR_TextureHotspots::getRawData()						const	{ return getDisplaySource()->getRawData();}
TextureStereoType	WVR_TextureHotspots::getStereoType()					const	{ return getDisplaySource()->getStereoType();}
TextureAlphaType	WVR_TextureHotspots::getAlphaType()						const	{ return getDisplaySource()->getAlphaType();}
WVR_Status			WVR_TextureHotspots::setDisplaySource(WVR_Texture *source)		{ if (!source) return wvrInvalidEntityName; pImpl->persistent.displaySource = source; return wvrSuccess;}
WVR_Texture*		WVR_TextureHotspots::getDisplaySource()					const	{ return pImpl->persistent.displaySource;}
WVR_Status			WVR_TextureHotspots::setHotspotSource(WVR_Texture *source)		{ if (!source)  return wvrInvalidEntityName; pImpl->persistent.hotspotSource = source; return wvrSuccess;}
WVR_Texture*		WVR_TextureHotspots::getHotspotSource()					const	{ return pImpl->persistent.hotspotSource;}
WVR_Status			WVR_TextureHotspots::addHotspotKey(const WVR_HotspotKey &key)	{ pImpl->persistent.hotspotKeys.push_back(key); return wvrSuccess;}
const dynamic_array<WVR_HotspotKey>& WVR_TextureHotspots::getHotspotKeys()	const	{ return pImpl->persistent.hotspotKeys;}

void WVR_TextureHotspots::cleanup() {
    pImpl->persistent.displaySource = nullptr;
    pImpl->persistent.hotspotSource = nullptr;
    pImpl->persistent.hotspotKeys.clear();
}
//-----------------------------------------------------------------------------
//
//-----------------------------------------------------------------------------

#define WVR_MAGIC_NUMBER			0x2E575665
//#define WVR_CURRENT_VERSION		0x00010001	// Initial version
//#define WVR_CURRENT_VERSION		0x00010002	// SimpleVersion scheme for sub-structures
#define WVR_CURRENT_VERSION			0x00010004	// Renderable writes out own version number

struct WVR_FileHeader {
	// file identifiers
	uint32	magic;
	uint32	version;
	uint32	wvrItemCount;
	WVR_FileHeader() : magic(WVR_MAGIC_NUMBER), version(WVR_CURRENT_VERSION), wvrItemCount(0) {}
	bool valid() const {
		return magic == WVR_MAGIC_NUMBER && version <= WVR_CURRENT_VERSION;
	}
	bool valid_loose() const {
		return magic == WVR_MAGIC_NUMBER
			|| magic == 0x2E575665
			|| magic == 0x2E575664
			|| magic == 0x2E575663
			|| magic == 0x2E575662;
	}
};

struct WVR_FileEntity {
	uint16			type;
	uint16			baseVersion;
	uint16			version;
	packed<uint64>	size;
};

struct WVR_FileEntities {
	string					name;				//!< The name of the file.
	uint32					magicNumber;		//!< The magic number used for this file.
	uint32					versionNumber;		//!< The file format version number.
	dynamic_array<WVR_FileEntity>	entities;
};

template<typename T> struct Map :hash_map<WVR_UUID, shared_ptr<T> > {};
template<typename T> struct Array : dynamic_array<shared_ptr<T> > {};

struct WVR_MetaData {
	string					title;
	string					subject;
	string					description;
	string					author;
	string					publisher;
	WVR_Date				date;
	string					version;
	string					language;
	string					rating;
	WVR_UUID				guid;
	uint8					videoCapability;
	uint8					computeCapability;
	map<string, string>		optional;
	WVR_MetaData() : videoCapability(0), computeCapability(0) {}
};
struct WVR_FileContents {
	WVR_MetaData				metadata;
	hash_map<string, shared_ptr<WVR_Item> >	nameMap;
	Map<WVR_Item>				uuidMap;
	Map<WVR_Renderable>			allRenderables;
	Map<WVR_Script>				allScripts;
	Map<WVR_Texture>			allTextures;
	Map<WVR_Geometry>			allGeometry;
	Map<WVR_AudioBank>			allAudioBanks;
	Array<WVR_Item>				writeOrder;
	Array<WVR_Scene>			allScenes;

	Map<WVR_Renderable>			&get_map(WVR_Renderable*)		{ return allRenderables; }
	Map<WVR_Script>				&get_map(WVR_Script*)			{ return allScripts; }
	Map<WVR_Texture>			&get_map(WVR_Texture*)			{ return allTextures; }
	Map<WVR_Geometry>			&get_map(WVR_Geometry*)			{ return allGeometry; }
	Map<WVR_AudioBank>			&get_map(WVR_AudioBank*)		{ return allAudioBanks; }

	bool	canNameBeAdded(const string &inName) const;
	void	add_item(WVR_Item *item);
	template<class T> void add_base(const shared_ptr<T> &item);
	template<class T> void add(const shared_ptr<T> &item) {
		add_base(item);
	}

	bool	hasName(const string &strName) const {
		return nameMap.check(strName);
	}

	WVR_Item* getName(const string &strName) {
		if (hasName(strName))
			return nameMap[strName].put();
		return 0;
	}

	WVR_Item *get(const WVR_UUID &id) {
		if (auto *p = uuidMap.check(id))
			return *p;
		return 0;
	}

	template<class T> T *get(const WVR_UUID &id) {
		if (auto *p = get_map((T*)0).check(id))
			return static_cast<T*>(p->get());
		return 0;
	}

	~WVR_FileContents() {
	}
};

bool WVR_FileContents::canNameBeAdded(const string &inName) const {
	// remove white space at beginning and end
	string	strName = trim(inName);

	// first check to see if the name follows the rules.
	if (strName.length() == 0)
		return false;

	if (strName.find(char_set(" \t\n\v\f\r")))
		return false;

	// it follows the rules, but is it already in the file?
	return !nameMap.check(strName);
}

void WVR_FileContents::add_item(WVR_Item *item) {
	// check the name to make sure it is unique
	if (!canNameBeAdded(item->getPimpl()->getName()))
		throw(wvrInvalidEntityName);

	// add it to the directories
	uuidMap[item->getPimpl()->getId()]	= item;
	nameMap[item->getPimpl()->getName()]= item;
}

template<class T> void WVR_FileContents::add_base(const shared_ptr<T> &item) {
	add_item(item);
    get_map(item)[item->getPimpl()->getId()]	= item.get();
    //writeOrder.insert(writeOrder.begin(), r);
}

template<> void WVR_FileContents::add(const shared_ptr<WVR_Scene> &item) {
	add_base(item);
	allScenes.push_back(item);
}

class WVR_Reader {
	static uint16		cCurrentBaseVersion;

	istream_ref			fp;
	WVR_FileContents	&info;
	bool				amBigEndian;
	WVR_Status			lastStatus;
	uint32				header_version;
	uint16				base_version;

	template<typename T>	bool	read(T &t)			{ return fp.read(t);	}
	void							read(string &out);
	void							read(WVR_UUID &out);

	template <typename T>	T*		allocate() {
		T	*item = new T();
		item->pImpl->setId(WVR_UUID().setRandom());
		return item;
	}

	void	doReadScriptArguments(WVR_ScriptArguments &);

	void	doRead(WVR_AudioBank*, uint16 version, uint16 baseVersion);
	void	doRead(WVR_Group*, uint16 version, uint16 baseVersion);
	void	doRead(WVR_Image*, uint16 version, uint16 baseVersion);
	void	doRead(WVR_Movie*, uint16 version, uint16 baseVersion);
	void	doRead(WVR_ProjScreen*, uint16 version, uint16 baseVersion);
	void	doRead(WVR_Scene*, uint16 version, uint16 baseVersion);
	void	doRead(WVR_Script*, uint16 version, uint16 baseVersion);
	void	doRead(WVR_SwitchRenderable*, uint16 version, uint16 baseVersion);
	void	doRead(WVR_TriMesh*, uint16 version, uint16 baseVersion);
	void	doRead(const shared_ptr<WVR_TextureHotspots>&, uint16 version, uint16 baseVersion);

	void	doReadEventHandler(WVR_EventHandler & handler);
	void	doReadVersionedCommon(WVR_Item *item, uint16 baseVersion);
	void	doReadRenderableData(WVR_Renderable_Impl &n);

	uint16	doReadSimpleVersion();

	void	doReadTransform(WVR_Transform &);
	void	doReadGeomVertex(WVR_GeomVertex &);
	void	doReadGeomAABox(WVR_GeomAABox &);
	void	doReadGeomObject(WVR_GeomObject &);
	void	doReadGeomModel(WVR_GeomModel &);
	void	doReadGeomModelIndexList(shared_ptr<WVR_IndexBuffer> &);
	void	doReadGeomModelVertexList(shared_ptr<WVR_VertexBuffer> &);
	void	doReadMovieAudioTrack(WVR_AudioTrackDescriptor &, uint16 version);
	void	doReadScriptInstance(shared_ptr<WVR_ScriptInstance> &, WVR_Item *par, const WVR_UUID &parUUID);
	void	doReadSoundEntry(WVR_SoundEntry &);

	shared_ptr<WVR_Renderable>	doReadUUIDasRenderable();
	shared_ptr<WVR_AudioBank>	doReadUUIDasAudioBank();
	shared_ptr<WVR_Geometry>	doReadUUIDasGeometry();
	shared_ptr<WVR_Script>		doReadUUIDasScript();
	shared_ptr<WVR_TriMesh>		doReadUUIDasTriMesh();
	shared_ptr<WVR_Texture>		doReadUUIDasTexture();
	shared_ptr<WVR_Item>		doReadUUIDasItem();

public:
	void	doRead(WVR_MetaData &md);

	static uint16	getCurrentBaseVersion() { return cCurrentBaseVersion; }
	void			Read();
	void			Scan(WVR_FileEntities &entities);
	WVR_Reader(istream_ref _fp, WVR_FileContents &_info, uint32 _header_version) : fp(_fp), info(_info), header_version(_header_version) {}
};

uint16	WVR_Reader::cCurrentBaseVersion = 2;

void WVR_Reader::doRead(WVR_MetaData &md) {
	read(md.title);
	read(md.subject);
	read(md.description);
	read(md.author);
	read(md.publisher);
	read(md.date.year);
	read(md.date.month);
	read(md.date.day);
	read(md.version);
	read(md.language);
	read(md.rating);
	read(md.guid);
	read(md.videoCapability);
	read(md.computeCapability);

	uint32 n;
    read(n);
    for (uint32 i = 0; i < n; ++i) {
        string key, value;
        read(key);
        read(value);
        md.optional[key] = value;
    }
}

void WVR_Reader::read(string &out) {
	uint32 len;
	read(len);
	if (len > 0) {
		fp.readbuff(out.alloc(len), len);
		out[len] = 0;
	} else {
		out.clear();
	}
}

void WVR_Reader::read(WVR_UUID &out) {
	string tmpString;
	read(tmpString);
	out = tmpString;
}

void WVR_Reader::Scan(WVR_FileEntities &entities) {
	// grab the number of entities to read and then loop through reading them
	uint32 itemCount;
	read(itemCount);

	while (itemCount--) {
		WVR_FileEntity entityInfo;
		read(entityInfo);

		// If there were problems reading, we would have thrown an exception and
		// not gotten here. So, don't try to make it more efficient by puttin it
		// into the list before we have done the read.
		entities.entities.push_back(entityInfo);

		// Skip the content because we are only interested in gathering the header information
		if (entityInfo.size > 0)
			fp.seek_cur(entityInfo.size);
	}
}

void WVR_Reader::Read() {
	if (header_version >= 0x00010004 ) // This is when the required metadata was introduced.
		doRead(info.metadata);

	// grab the number of entities to read and then loop through reading them
	uint32 itemCount;
	read(itemCount);

	while (itemCount--) {
		// first read the object type/version and size information. Based on that
		// we will make a determination as to whether to read the object or not.
		WVR_FileEntity entity;
		read(entity);

		uint16 version		= entity.version;
		uint16 baseVersion	= entity.baseVersion;

		// check the object type and decide whether to read or skip
		bool doSkip = entity.baseVersion > getCurrentBaseVersion();
		if (!doSkip) switch (entity.type) {
			case WVR_TYPE_ID_SCENE:				doSkip = version > WVR_Scene_Impl::cCurrentVersion;				break;
			case WVR_TYPE_ID_SCRIPT:			doSkip = version > WVR_Script_Impl::cCurrentVersion;			break;
			case WVR_TYPE_ID_GROUP:				doSkip = version > WVR_Group_Impl::cCurrentVersion;				break;
			case WVR_TYPE_ID_SWITCHRENDERABLE:	doSkip = version > WVR_SwitchRenderable_Impl::cCurrentVersion;	break;
			case WVR_TYPE_ID_TRIMESH:			doSkip = version > WVR_TriMesh_Impl::cCurrentVersion;			break;
			case WVR_TYPE_ID_PROJSCREEN:		doSkip = version > WVR_ProjScreen_Impl::cCurrentVersion;		break;
			case WVR_TYPE_ID_MOVIE:				doSkip = version > WVR_Movie_Impl::cCurrentVersion;				break;
			case WVR_TYPE_ID_IMAGE:				doSkip = version > WVR_Image_Impl::cCurrentVersion;				break;
			case WVR_TYPE_ID_AUDIOBANK:			doSkip = version > WVR_AudioBank_Impl::cCurrentVersion;			break;
			case WVR_TYPE_ID_TEXTUREHOTSPOTS:	doSkip = version > WVR_TextureHotspots_Impl::cCurrentVersion;	break;
			case WVR_TYPE_ID_FILE:
			default:
				// TODO: currently we do not support nested files
				doSkip = true;
				break;
		}

		// if we are going to skip, we need to move the file pointer forward by the skip amount
		if (doSkip) {
			if (entity.size > 0) {
				fp.seek_cur(entity.size);
				continue;
			}
		}

		// based on the type of object in the header we have to allocate the object and then read it.
		switch (entity.type) {
			case WVR_TYPE_ID_SCENE: {
				shared_ptr<WVR_Scene> newScene = allocate<WVR_Scene>();
				doRead(newScene, version, baseVersion);
				info.add(newScene);
				break;
			}
			case WVR_TYPE_ID_SCRIPT: {
				shared_ptr<WVR_Script> newScript = allocate<WVR_Script>();
				doRead(newScript, version, baseVersion);
				info.add(newScript);
				break;
			}
			case WVR_TYPE_ID_GROUP: {
				shared_ptr<WVR_Group> newGroup = allocate<WVR_Group>();
				doRead(newGroup, version, baseVersion);
				info.add(newGroup);
				break;
			}
			case WVR_TYPE_ID_SWITCHRENDERABLE: {
				shared_ptr<WVR_SwitchRenderable> newSwitchRenderable = allocate<WVR_SwitchRenderable>();
				doRead(newSwitchRenderable, version, baseVersion);
				info.add(newSwitchRenderable);
				break;
			}
			case WVR_TYPE_ID_TRIMESH: {
				shared_ptr<WVR_TriMesh> newTriMesh = allocate<WVR_TriMesh>();
				doRead(newTriMesh, version, baseVersion);
				info.add(newTriMesh);
				break;
			}
			case WVR_TYPE_ID_PROJSCREEN: {
				shared_ptr<WVR_ProjScreen> newProjScreen = allocate<WVR_ProjScreen>();
				doRead(newProjScreen, version, baseVersion);
				info.add(newProjScreen);
				break;
			}
			case WVR_TYPE_ID_MOVIE: {
				shared_ptr<WVR_Movie> newMovie = allocate<WVR_Movie>();
				doRead(newMovie, version, baseVersion);
				info.add(newMovie);
				break;
			}
			case WVR_TYPE_ID_IMAGE: {
				shared_ptr<WVR_Image> newImage = allocate<WVR_Image>();
				doRead(newImage, version, baseVersion);
				info.add(newImage);
				break;
			}
			case WVR_TYPE_ID_AUDIOBANK: {
				shared_ptr<WVR_AudioBank> newAudioBank = allocate<WVR_AudioBank>();
				doRead(newAudioBank, version, baseVersion);
				info.add(newAudioBank);
				break;
			}
			case WVR_TYPE_ID_TEXTUREHOTSPOTS: {
				shared_ptr<WVR_TextureHotspots> newHotspots = allocate<WVR_TextureHotspots>();
				doRead(newHotspots, version, baseVersion);
				info.add(newHotspots);
				break;
			}
			default:
				throw(wvrReaderUnknownEntity);
		}
	}
}

void WVR_Reader::doRead(WVR_AudioBank* out, uint16 version, uint16 baseVersion) {
	WVR_AudioBank_Impl &imp = reinterpret_cast<WVR_AudioBank_Impl &>(*(out->getPimpl()));

	// The non-header data defined in the parent WVR_VersionedImpl class
	doReadVersionedCommon(out, baseVersion);

	uint32 numChildren;
	read(numChildren);
	imp.persistent.audioTracks.clear();
	imp.persistent.audioTracks.reserve(numChildren);
	for (uint32 i = 0; i < numChildren; i++) {
		imp.persistent.audioTracks.push_back();
		doReadSoundEntry(imp.persistent.audioTracks[i]);
	}
}

void WVR_Reader::doRead(WVR_Group* out, uint16 version, uint16 baseVersion) {
	// grab an easy reference to implementation because we use it everywhere
	WVR_Group_Impl &imp = reinterpret_cast<WVR_Group_Impl &>(*(out->getPimpl()));

	// The non-header data defined in the parent WVR_VersionedImpl class
	doReadVersionedCommon(out, baseVersion);

	// we are a renderable
	doReadRenderableData(imp);

	uint32 numChildren;
	read(numChildren);

	// read the private data
	for (uint32 i = 0; i < numChildren; i++) {
		shared_ptr<WVR_Renderable> p = doReadUUIDasRenderable();
		if (p)
			imp.addChild(p);
	}
}

void WVR_Reader::doRead(WVR_Image*obj, uint16 version, uint16 baseVersion) {
	// grab an easy reference to implementation because we use it often
	WVR_Image_Impl &imp = reinterpret_cast<WVR_Image_Impl &>(*(obj->getPimpl()));

	// The non-header data defined in the parent WVR_VersionedImpl class
	doReadVersionedCommon(obj, baseVersion);

	// read the private data for this node
	read(imp.persistent.pixelFormat);
	read(imp.persistent.width);
	read(imp.persistent.height);
	read(imp.persistent.strideBytes);
	read(imp.persistent.dataBytes);
	uint32 tmpu32;
	read(tmpu32);
	imp.persistent.stereoImageType = (TextureStereoType)tmpu32;
	read(tmpu32);
	imp.persistent.alphaImageType = (TextureAlphaType)tmpu32;

	if (imp.persistent.dataBytes > 0) {
		uint64 allocSize = (imp.persistent.dataBytes + sizeof(uint64)) / sizeof(uint64);
		// TODO the following will fail for sizes greater than 2^32

		if (version < 2) {
			// Uncompressed image.
			imp.persistent.imageData = new uint64[(unsigned int)allocSize];
			fp.readbuff( imp.persistent.imageData,imp.persistent.dataBytes);
		} else {
			// Snappy compressed image.
			void * compressedImage = new uint64[(unsigned int)allocSize];
			fp.readbuff( compressedImage,imp.persistent.dataBytes);

			malloc_block uncompressedImage;
			bool uncompressValid = false;
			uncompressValid = snappy::Uncompress((char*)compressedImage, imp.persistent.dataBytes, uncompressedImage);

			if (!uncompressValid)
				throw(wvrSnappyUncompressError);

			imp.persistent.imageData = (void *)new int8[uncompressedImage.length()];
			memcpy(imp.persistent.imageData, uncompressedImage, uncompressedImage.length() * sizeof(int8));

			delete[](uint64*)(compressedImage);
		}
	} else {
		imp.persistent.imageData = nullptr;
	}
}

void WVR_Reader::doRead(WVR_Movie*obj, uint16 version, uint16 baseVersion) {
	// grab an easy reference to implementation because we use it everywhere
	WVR_Movie_Impl &imp = reinterpret_cast<WVR_Movie_Impl &>(*(obj->getPimpl()));

	// The non-header data defined in the parent WVR_VersionedImpl class
	doReadVersionedCommon(obj, baseVersion);

	// read the video data
	uint16 tmp = 0;
	read(tmp);
	imp.persistent.validData = tmp != 0;
	read(tmp);
	imp.persistent.startActive = tmp != 0;
	if (version >= 3) {
		read(tmp);
		imp.persistent.exitOnComplete = tmp != 0;
    }
	read(imp.persistent.mp4Size);
	read(imp.persistent.startingOrientation);
	read(imp.persistent.loop);

	read(imp.persistent.movieFlags);
	uint32 audioTrackCount;
	read(audioTrackCount);
	imp.persistent.audioTracks.clear();
	imp.persistent.audioTracks.reserve(audioTrackCount);
	for (uint32 i = 0; i < audioTrackCount; i++) {
		imp.persistent.audioTracks.push_back();
		doReadMovieAudioTrack(imp.persistent.audioTracks[i], version);
	}
	uint32 tmpu32;
	read(tmpu32);
	imp.persistent.stereoImageType = (TextureStereoType)tmpu32;
	read(tmpu32);
	imp.persistent.alphaImageType = (TextureAlphaType)tmpu32;

	// skip over the actual video data. We don't need it right now.
	imp.runtime.mp4OffsetInFile = fp.tell();

	fp.readbuff(imp.mp4Data.create(imp.persistent.mp4Size), imp.persistent.mp4Size);

//	if (imp.persistent.mp4Size > 0)
//		fp.seek_cur(imp.persistent.mp4Size);
}

void WVR_Reader::doRead(WVR_ProjScreen*obj, uint16 version, uint16 baseVersion) {
	// grab an easy reference to implementation because we use it everywhere
	WVR_ProjScreen_Impl &imp = reinterpret_cast<WVR_ProjScreen_Impl &>(*(obj->getPimpl()));

	// The non-header data defined in the parent WVR_VersionedImpl class
	doReadVersionedCommon(obj, baseVersion);

	// we are a renderable
	doReadRenderableData(imp);

	// our private data
	imp.persistent.projectionSurface = doReadUUIDasTriMesh();
	imp.persistent.projectionTextures = doReadUUIDasTexture();
}


void WVR_Reader::doRead(WVR_Scene*obj, uint16 version, uint16 baseVersion) {
	// grab an easy reference to implementation because we use it everywhere
	WVR_Scene_Impl &imp = reinterpret_cast<WVR_Scene_Impl &>(*(obj->getPimpl()));

	// The non-header data defined in the parent WVR_VersionedImpl class
	doReadVersionedCommon(obj, baseVersion);

	// we are a renderable
	doReadRenderableData(imp);

	// the root node is just a UUid
	imp.setSceneRoot(doReadUUIDasRenderable());
	if (version >= 2)
		imp.setAudioBank(doReadUUIDasAudioBank());
}

void WVR_Reader::doRead(WVR_Script*obj, uint16 version, uint16 baseVersion) {
	// grab an easy reference to implementation because we use it everywhere
	WVR_Script_Impl &imp = reinterpret_cast<WVR_Script_Impl &>(*(obj->getPimpl()));

	// The non-header data defined in the parent WVR_VersionedImpl class
	doReadVersionedCommon(obj, baseVersion);

	// Read the length of the bytecode
	read(imp.persistent.scriptSource);
	imp.persistent.amActionScript = false;
	if (version >= 2) {
		uint16 tmp;
		read(tmp);
		imp.persistent.amActionScript = tmp != 0;
	}
}

void WVR_Reader::doRead(WVR_SwitchRenderable* out, uint16 version, uint16 baseVersion) {
	// grab an easy reference to implementation because we use it everywhere
	WVR_SwitchRenderable_Impl &imp = reinterpret_cast<WVR_SwitchRenderable_Impl &>(*(out->getPimpl()));

	// The non-header data defined in the parent WVR_VersionedImpl class
	doReadVersionedCommon(out, baseVersion);

	// we are a renderable
	doReadRenderableData(imp);

	// read the private data
	uint32 numChildren;
	read(numChildren);

	for (uint32 i = 0; i < numChildren; i++) {
		shared_ptr<WVR_Renderable> p = doReadUUIDasRenderable();
		if (p) {
			imp.addChild(p);
		}
	}
}

void WVR_Reader::doRead(WVR_TriMesh* out, uint16 version, uint16 baseVersion) {
	// grab an easy reference to implementation because we use it everywhere
	WVR_TriMesh_Impl &imp = reinterpret_cast<WVR_TriMesh_Impl &>(*(out->getPimpl()));

	// The non-header data defined in the parent WVR_VersionedImpl class
	doReadVersionedCommon(out, baseVersion);

	// read all the data specific to our type
	doReadGeomModel(imp.persistent.theModel);
	if (version > 1) {
		read(imp.persistent.flags);
	} else {
		imp.persistent.flags = 0;
	}

}

void WVR_Reader::doRead(const shared_ptr<WVR_TextureHotspots>&out, uint16 version, uint16 baseVersion) {
	// grab an easy reference to implementation because we use it everywhere
	WVR_TextureHotspots_Impl &imp = reinterpret_cast<WVR_TextureHotspots_Impl &>(*(out->getPimpl()));

	// The non-header data defined in the parent WVR_VersionedImpl class
	doReadVersionedCommon(out, baseVersion);

	imp.persistent.displaySource = doReadUUIDasTexture();
	imp.persistent.hotspotSource = doReadUUIDasTexture();

	uint8 uKeyCount;
	read(uKeyCount);
	for (uint8 uIdx = 0; uIdx < uKeyCount; ++uIdx) {
		WVR_HotspotKey newKey;
		read(newKey.a);
		read(newKey.b.r);
		read(newKey.b.g);
		read(newKey.b.b);
		read(newKey.b.a);
		imp.persistent.hotspotKeys.push_back(newKey);
	}
}

void WVR_Reader::doReadRenderableData(WVR_Renderable_Impl &imp) {
	uint16 version = 1;

	if (header_version >= 0x00010003)
		read(version);

	if (version < 5) {
		read(imp.persistent.startTransform.rotation);
		read(imp.persistent.startTransform.position);
		if (version >= 3) {
			read(imp.persistent.startTransform.scale);
			if (version < 4)
				imp.persistent.startTransform.scale = one;
		} else {
			imp.persistent.startTransform.scale = one;
		}
	} else {
		doReadTransform(imp.persistent.startTransform);
	}

	read(imp.persistent.boundingBox);
	if (version >= 2) {
		read(imp.persistent.flags);
	} else {
		imp.persistent.flags = 0;
	}
}

void WVR_Reader::doReadEventHandler(WVR_EventHandler & handler) {
	uint16 version = doReadSimpleVersion();
	if (version > handler.cCurrentVersion)
		throw(wvrFileReadNewVersion);

	read(handler.eventFilter);

	uint32 actionCount = 1;
	if (version >= 2)
		read(actionCount);

	for (uint32 c = 0; c < actionCount; c++) {
		string tmp;
		read(tmp);
		handler.actionList.push_back(tmp);
	}
}


void WVR_Reader::doReadVersionedCommon(WVR_Item *item, uint16 baseVersion) {
	WVR_VersionedImpl &imp = *(item->getPimpl());

	read(imp.instanceId);
	read(imp.instanceName);
	doReadScriptInstance(imp.assignedScript, item, imp.instanceId);


    if (baseVersion >= 2)  {
        uint32 count;
        read(count);
        for (uint32 i = 0; i < count; i++)
            doReadEventHandler(imp.eventHandlers.push_back());
    }
}

uint16 WVR_Reader::doReadSimpleVersion() {
	// the first version did not write out any data to read
	if (header_version == 0x00010001)
		return 1;

	uint16 tmp;
	read(tmp);
	return tmp;
}

void WVR_Reader::doReadScriptArguments(WVR_ScriptArguments &map) {
	map.clear();

	uint32 count;
	read(count);
	for (uint32 i = 0; i < count; i++) {
		string first;
		string second;
		read(first);
		read(second);

		map[first] = second;
	}
}

void WVR_Reader::doReadScriptInstance(shared_ptr<WVR_ScriptInstance> &instance, WVR_Item *par, const WVR_UUID &parUUID) {
	uint16 version = doReadSimpleVersion();
	if (version > instance->cCurrentVersion)
		throw(wvrFileReadNewVersion);

	shared_ptr<WVR_Script> scr = doReadUUIDasScript();
	if (!scr) {
		instance = shared_ptr<WVR_ScriptInstance>();
		return;
	}

	// the next item is the UUID of the bound object, which could be the parent
	// object that is reading this instance, in which case it won't be in the
	// directories yet. So we read the UUID, check if its the parent, and only
	// try to resolve it from the directories if its not the parent.
	WVR_UUID tmpUuid;
	shared_ptr<WVR_Item> bn;
	read(tmpUuid);
	if (tmpUuid == parUUID) {
		bn = par;
	} else {
		if (!(tmpUuid == WVR_UUID())) {
			bn = info.get(tmpUuid);
			if (!bn)
				throw(wvrFileReadUnresolved);
		} else {
			instance = shared_ptr<WVR_ScriptInstance>();
			return;
		}
	}

	WVR_ScriptArguments arg;
	doReadScriptArguments(arg);

	instance = shared_ptr<WVR_ScriptInstance>(new WVR_ScriptInstance(bn, arg));
	instance->baseDefinition = scr;
}

shared_ptr<WVR_Renderable> WVR_Reader::doReadUUIDasRenderable() {
	WVR_UUID tmpUuid;
	read(tmpUuid);

	// if its a valid UUID, get the pointer or thrown an unresolved exception.
	// If its a nil UUID, just return a nil pointer, so that the caller can
	// decide what to do.
	if (!(tmpUuid == WVR_UUID())) {
		shared_ptr<WVR_Renderable> p = info.get<WVR_Renderable>(tmpUuid);
		if (p)
			return p;
		throw(wvrFileReadUnresolved);
	} else {
		return shared_ptr<WVR_Renderable>();
	}
}

void WVR_Reader::doReadTransform(WVR_Transform &tr) {
	uint16 version = doReadSimpleVersion();
	if (version > tr.cCurrentVersion) {
		throw(wvrFileReadNewVersion);
	}
	float3p		v3;
	float4p		v4;

	read(v3);
	tr.position	= v3;
	read(v3);
	tr.scale	= v3;
	read(v4);
	tr.rotation	= float4(v4);
}

void WVR_Reader::doReadGeomVertex(WVR_GeomVertex &gv) {
	uint16 version = doReadSimpleVersion();
	if (version > gv.cCurrentVersion)
		throw(wvrFileReadNewVersion);

	read(gv.p);
	read(gv.n);
	read(gv.t);
}

void WVR_Reader::doReadGeomAABox(WVR_GeomAABox &box) {
	uint16 version = doReadSimpleVersion();
	if (version > box.cCurrentVersion)
		throw(wvrFileReadNewVersion);

	read(box.mini);
	read(box.maxi);
	read(box.valid);
}

/*
	Function call to read the index data.
*/
void WVR_Reader::doReadGeomModelIndexList(shared_ptr<WVR_IndexBuffer> &indexBuffer) {

	uint16 version = doReadSimpleVersion();
	if (version > indexBuffer->cCurrentVersion)
		throw(wvrFileReadNewVersion);

	// Read number of indices in index buffer
	uint32 numIndices;
	read(numIndices);

	indexBuffer->indexList.reserve(numIndices);

		// Read indices for each index buffer
	int32 index;
	for (uint32 iter = 0; iter < numIndices; ++iter) {
		read(index);
		indexBuffer->indexList.push_back(index);
	}
}

/*
	Function call to fill in Vertex data into Vertex buffer.
*/
void WVR_Reader::doReadGeomModelVertexList(shared_ptr<WVR_VertexBuffer> &vertexBuffer) {

	uint16 version = doReadSimpleVersion();
	if (version > vertexBuffer->cCurrentVersion)
		throw(wvrFileReadNewVersion);

	// Read number of Vertices in vertex buffer
	uint32 numVertices;
	read(numVertices);

	vertexBuffer->vertexList.reserve(numVertices);

	// Read GeomVertex for each vertex
	WVR_GeomVertex vertex;
	for (uint32 iter = 0; iter < numVertices; ++iter) {
		doReadGeomVertex(vertex);
		vertexBuffer->vertexList.push_back(vertex);
	}
}

void WVR_Reader::doReadMovieAudioTrack(WVR_AudioTrackDescriptor &audioTrack, uint16 parentVersion) {

	uint16 version = doReadSimpleVersion();
	if (version > audioTrack.cCurrentVersion)
		throw(wvrFileReadNewVersion);

	// for version 1 we used the parent version
	if (version == 1)
		version = parentVersion;

	if (version == 1)
		throw(wvrFileReadEntityVersionUnsupported);

	read(audioTrack.persistent.position);
	read(audioTrack.persistent.emitterDirection);
	read(audioTrack.persistent.emitterSpreadInner);
	read(audioTrack.persistent.emitterSpreadOuter);
	read(audioTrack.persistent.ambientFraction);
	read(audioTrack.persistent.volume);
	if (version < 2) {
		// audioTrack.trackType - deleted in version 2
		uint32 tmp;
		read(tmp);
	}
	read(audioTrack.persistent.flags);
	read(audioTrack.persistent.posNodeName);
	read(audioTrack.persistent.posNodeType);
	uint8 isPositional;
	read(isPositional);
	audioTrack.persistent.isPositional = (isPositional != 0 ? true : false);

	if (version >= 2) {
		read(audioTrack.persistent.minAttenuateDistance);
		read(audioTrack.persistent.maxAttenuateDistance);
	}
}

void WVR_Reader::doReadGeomObject(WVR_GeomObject &go) {

	uint16 version = doReadSimpleVersion();
	if (version > go.cCurrentVersion)
		throw(wvrFileReadNewVersion);

	doReadGeomModelIndexList(go.indBuf);
	doReadGeomModelVertexList(go.vertBuf);
	read(go.boundingBox);

}

/*
	Read the TriMesh Data for each object into index and vertex buffers.
*/
void WVR_Reader::doReadGeomModel(WVR_GeomModel &gm) {
	uint16 version = doReadSimpleVersion();
	if (version > gm.cCurrentVersion)
		throw(wvrFileReadNewVersion);

	// Read number of objects in file
	uint32 numObjects;
	read(numObjects);

	gm.objects.reserve(numObjects);

	// Read Index buffer and Vertex buffer for each object
	for (uint32 iterObjects = 0; iterObjects < numObjects; ++iterObjects) {

		// allocate the object
		shared_ptr<WVR_GeomObject> geomObject(new WVR_GeomObject);
		shared_ptr<WVR_VertexBuffer> vBuffer(new WVR_VertexBuffer);
		shared_ptr<WVR_IndexBuffer> iBuffer(new WVR_IndexBuffer);
		geomObject->indBuf = iBuffer;
		geomObject->vertBuf = vBuffer;

		doReadGeomObject(*geomObject);

		gm.objects.push_back(geomObject);
	}
}

void WVR_Reader::doReadSoundEntry(WVR_SoundEntry &sound) {
	uint16 version = doReadSimpleVersion();
	if (version > sound.cCurrentVersion)
		throw(wvrFileReadNewVersion);

	// the parent version for the following call will be ignored
	doReadMovieAudioTrack(sound.info, 0);
	read(sound.startActive);
	read(sound.loop);
	read(sound.fileType);
	read(sound.dataSize);
	sound.datav.create(sound.dataSize);
	fp.readbuff(sound.datav, sound.dataSize);
}

shared_ptr<WVR_Geometry> WVR_Reader::doReadUUIDasGeometry() {
	WVR_UUID tmpUuid;
	read(tmpUuid);

	// if its a valid UUID, get the pointer or thrown an unresolved exception.
	// If its a nil UUID, just return a nil pointer, so that the caller can
	// decide what to do.
	if (!(tmpUuid == WVR_UUID())) {
		shared_ptr<WVR_Geometry> p = info.get<WVR_Geometry>(tmpUuid);
		if (p)
			return p;
		throw(wvrFileReadUnresolved);
	} else {
		return shared_ptr<WVR_Geometry>();
	}
}

shared_ptr<WVR_Script> WVR_Reader::doReadUUIDasScript() {
	WVR_UUID tmpUuid;
	read(tmpUuid);

	// if its a valid UUID, get the pointer or thrown an unresolved exception.
	// If its a nil UUID, just return a nil pointer, so that the caller can
	// decide what to do.
	if (!(tmpUuid == WVR_UUID())) {
		shared_ptr<WVR_Script> p = info.get<WVR_Script>(tmpUuid);
		if (p)
			return p;
		throw(wvrFileReadUnresolved);
	} else {
		return shared_ptr<WVR_Script>();
	}
}

shared_ptr<WVR_TriMesh> WVR_Reader::doReadUUIDasTriMesh() {
	WVR_UUID tmpUuid;
	read(tmpUuid);

	// if its a valid UUID, get the pointer or thrown an unresolved exception.
	// If its a nil UUID, just return a nil pointer, so that the caller can
	// decide what to do.
	if (!(tmpUuid == WVR_UUID())) {
		shared_ptr<WVR_TriMesh> p = info.get<WVR_TriMesh>(tmpUuid);
		if (p)
			return p;
		throw(wvrFileReadUnresolved);
	} else {
		return shared_ptr<WVR_TriMesh>();
	}
}

shared_ptr<WVR_Texture> WVR_Reader::doReadUUIDasTexture() {
	WVR_UUID tmpUuid;
	read(tmpUuid);

	// if its a valid UUID, get the pointer or thrown an unresolved exception.
	// If its a nil UUID, just return a nil pointer, so that the caller can
	// decide what to do.
	if (!(tmpUuid == WVR_UUID())) {
		shared_ptr<WVR_Texture> p = info.get<WVR_Texture>(tmpUuid);
		if (p)
			return p;
		throw(wvrFileReadUnresolved);
	} else {
		return shared_ptr<WVR_Texture>();
	}
}

shared_ptr<WVR_Item> WVR_Reader::doReadUUIDasItem() {
	WVR_UUID tmpUuid;
	read(tmpUuid);

	// if its a valid UUID, get the pointer or thrown an unresolved exception.
	// If its a nil UUID, just return a nil pointer, so that the caller can
	// decide what to do.
	if (!(tmpUuid == WVR_UUID())) {
		shared_ptr<WVR_Item> p = info.get(tmpUuid);
		if (p)
			return p;
		throw(wvrFileReadUnresolved);
	} else {
		return shared_ptr<WVR_Item>();
	}
}

shared_ptr<WVR_AudioBank> WVR_Reader::doReadUUIDasAudioBank() {
	WVR_UUID tmpUuid;
	read(tmpUuid);

	// if its a valid UUID, get the pointer or thrown an unresolved exception.
	// If its a nil UUID, just return a nil pointer, so that the caller can
	// decide what to do.
	if (!(tmpUuid == WVR_UUID())) {
		shared_ptr<WVR_AudioBank> p = info.get<WVR_AudioBank>(tmpUuid);
		if (p)
			return p;
		throw(wvrFileReadUnresolved);
	} else {
		return shared_ptr<WVR_AudioBank>();
	}
}

//-----------------------------------------------------------------------------
//	basic types
//-----------------------------------------------------------------------------

template<WVR_TypeID id> struct WEVR_type;
template<> struct WEVR_type<WVR_TYPE_ID_SCENE>				{ typedef WVR_Scene				type; };
template<> struct WEVR_type<WVR_TYPE_ID_SCRIPT>				{ typedef WVR_Script			type; };
template<> struct WEVR_type<WVR_TYPE_ID_GROUP>				{ typedef WVR_Group				type; };
template<> struct WEVR_type<WVR_TYPE_ID_SWITCHRENDERABLE>	{ typedef WVR_SwitchRenderable	type; };
template<> struct WEVR_type<WVR_TYPE_ID_PROJSCREEN>			{ typedef WVR_ProjScreen		type; };
template<> struct WEVR_type<WVR_TYPE_ID_TRIMESH>			{ typedef WVR_TriMesh			type; };
template<> struct WEVR_type<WVR_TYPE_ID_MOVIE>				{ typedef WVR_Movie				type; };
template<> struct WEVR_type<WVR_TYPE_ID_IMAGE>				{ typedef WVR_Image				type; };
template<> struct WEVR_type<WVR_TYPE_ID_AUDIOBANK>			{ typedef WVR_AudioBank			type; };
template<> struct WEVR_type<WVR_TYPE_ID_TEXTUREHOTSPOTS>	{ typedef WVR_TextureHotspots	type; };
template<> struct WEVR_type<WVR_TYPE_ID_RENDERABLE>			{ typedef WVR_Renderable		type; };

template<WVR_TypeID id> ISO_ptr<void> getISO(const WVR_Item *i) {
	return ISO::MakePtr(i->getName(), ((typename WEVR_type<id>::type*)i)->pImpl);
}
ISO_ptr<void> WVR_Item::Deref() const {
	switch (getType()) {
		case WVR_TYPE_ID_SCENE:				return ::getISO<WVR_TYPE_ID_SCENE			>(this);
		case WVR_TYPE_ID_SCRIPT:			return ::getISO<WVR_TYPE_ID_SCRIPT			>(this);
		case WVR_TYPE_ID_GROUP:				return ::getISO<WVR_TYPE_ID_GROUP			>(this);
		case WVR_TYPE_ID_SWITCHRENDERABLE:	return ::getISO<WVR_TYPE_ID_SWITCHRENDERABLE>(this);
		case WVR_TYPE_ID_PROJSCREEN:		return ::getISO<WVR_TYPE_ID_PROJSCREEN		>(this);
		case WVR_TYPE_ID_TRIMESH:			return ::getISO<WVR_TYPE_ID_TRIMESH			>(this);
		case WVR_TYPE_ID_MOVIE:				return ::getISO<WVR_TYPE_ID_MOVIE			>(this);
		case WVR_TYPE_ID_IMAGE:				return ::getISO<WVR_TYPE_ID_IMAGE			>(this);
		case WVR_TYPE_ID_AUDIOBANK:			return ::getISO<WVR_TYPE_ID_AUDIOBANK		>(this);
		case WVR_TYPE_ID_TEXTUREHOTSPOTS:	return ::getISO<WVR_TYPE_ID_TEXTUREHOTSPOTS	>(this);
//		case WVR_TYPE_ID_RENDERABLE:		return ::getISO<WVR_TYPE_ID_RENDERABLE		>(this);
		default: return ISO_NULL;
	}
}

template<> struct ISO::def<WVR_Item>		: ISO::TypeUserVirt<WVR_Item>		{ def() : ISO::TypeUserVirt<WVR_Item>		("WVR_Item")		{} };
template<> struct ISO::def<WVR_Renderable>	: ISO::TypeUserVirt<WVR_Renderable>	{ def() : ISO::TypeUserVirt<WVR_Renderable>	("WVR_Renderable")	{} };
template<> struct ISO::def<WVR_Texture>		: ISO::TypeUserVirt<WVR_Texture>	{ def() : ISO::TypeUserVirt<WVR_Texture>	("WVR_Texture")		{} };

ISO_DEFSAME(WVR_UUID, GUID);
ISO_DEFUSERCOMPV(WVR_ColorRGBA, r, g, b, a);
ISO_DEFUSERCOMPV(WVR_Date, year, month, day);
ISO_DEFUSERCOMPV(WVR_Transform, position, rotation, scale);

ISO_DEFUSERENUMV(TextureStereoType, eTextureStereoTypeMono, eTextureStereoTypeOULR, eTextureStereoTypeOURL, eTextureStereoTypeSSLR, eTextureStereoTypeSSRL, eTextureStereoTypeCount);
ISO_DEFUSERENUMV(TextureAlphaType, eTextureAlphaTypeNone, eTextureAlphaTypePixel, eTextureAlphaTypeOUCA, eTextureAlphaTypeOUAC, eTextureAlphaTypeSSCA, eTextureAlphaTypeSSAC, eTextureAlphaTypeCount);

ISO_DEFUSERCOMPV(WVR_MetaData,title, subject, description, author, publisher, date, version, language, rating, guid, videoCapability, computeCapability, optional);

ISO_DEFCOMPV(WVR_Script_Impl::Persistent,scriptSource);
ISO_DEFUSERCOMPV(WVR_Script_Impl, persistent);
ISO_DEFUSERCOMPV(WVR_Script, pImpl);
ISO_DEFUSERCOMPV(WVR_ScriptInstance, baseDefinition, arguments);

ISO_DEFUSERCOMPV(WVR_VersionedImpl, instanceId, assignedScript);

ISO_DEFCOMPV(WVR_Movie_Impl::Persistent, validData, startActive, mp4Size, startingOrientation, movieFlags, stereoImageType, alphaImageType, audioTracks, loop);
ISO_DEFUSERCOMPV(WVR_Movie_Impl, persistent, assignedScript, mp4Data);

ISO_DEFCOMPV(WVR_Renderable_Impl::Persistent,startTransform, flags);
ISO_DEFUSERCOMPV(WVR_Renderable_Impl, persistent);

ISO_DEFCOMPV(WVR_Group_Impl::Persistent,children);
ISO_DEFUSERCOMPV(WVR_Group_Impl, WVR_Renderable_Impl::persistent, persistent);

ISO_DEFCOMPV(WVR_Scene_Impl::Persistent,rootNode, audioBank);
ISO_DEFUSERCOMPV(WVR_Scene_Impl, WVR_Renderable_Impl::persistent, persistent);
ISO_DEFUSERCOMPV(WVR_Scene, pImpl);

ISO_DEFCOMPV(WVR_SwitchRenderable_Impl::Persistent,children);
ISO_DEFUSERCOMPV(WVR_SwitchRenderable_Impl, WVR_Renderable_Impl::persistent, persistent);

ISO_DEFCOMPV(WVR_ProjScreen_Impl::Persistent,projectionTextures, projectionSurface);
ISO_DEFUSERCOMPV(WVR_ProjScreen_Impl, WVR_Renderable_Impl::persistent, persistent);

ISO_DEFUSERCOMPV(WVR_GeomVertex, p, n, t);
ISO_DEFUSERCOMPV(WVR_VertexBuffer, vertexList);
ISO_DEFUSERCOMPV(WVR_IndexBuffer, indexList);
ISO_DEFUSERCOMPV(WVR_GeomObject, vertBuf, indBuf);
ISO_DEFUSERCOMPV(WVR_GeomModel, objects);
ISO_DEFCOMPV(WVR_TriMesh_Impl::Persistent,theModel, flags);
ISO_DEFUSERCOMPBV(WVR_TriMesh_Impl, WVR_VersionedImpl, persistent);
ISO_DEFUSERCOMPV(WVR_TriMesh, pImpl);

ISO_DEFCOMPV(WVR_Image_Impl::Persistent,pixelFormat, width, height, strideBytes, dataBytes, stereoImageType, alphaImageType);
ISO_DEFUSERCOMPBV(WVR_Image_Impl, WVR_VersionedImpl, persistent);

ISO_DEFCOMPV(WVR_AudioTrackDescriptor::Persistent, position, emitterDirection, emitterSpreadInner, emitterSpreadOuter, ambientFraction, volume, flags, posNodeName, posNodeType, isPositional, minAttenuateDistance, maxAttenuateDistance);
ISO_DEFUSERCOMPV(WVR_AudioTrackDescriptor, persistent);
ISO_DEFUSERCOMPV(WVR_SoundEntry, info, startActive, loop, fileType, datav);
ISO_DEFCOMPV(WVR_AudioBank_Impl::Persistent,audioTracks);
ISO_DEFUSERCOMPBV(WVR_AudioBank_Impl, WVR_VersionedImpl, persistent);
ISO_DEFUSERCOMPV(WVR_AudioBank, pImpl);

ISO_DEFCOMPV(WVR_TextureHotspots_Impl::Persistent,displaySource, hotspotSource, hotspotKeys);
ISO_DEFUSERCOMPBV(WVR_TextureHotspots_Impl, WVR_VersionedImpl, persistent);

template<typename T> struct ISO::def<Map<T> > : ISO::VirtualT2<Map<T> > {
	typedef Map<T> type;
	static uint32		Count(type &a)					{ return uint32(a.size());	}
	static ISO::Browser2	Index(type &a, int i)			{
		auto it = a.begin();
		while (i--)
			++it;
		return (*it)->Deref();
	}
};

ISO_DEFUSERCOMPV(WVR_FileContents, allRenderables, allScripts, allTextures, allGeometry, allAudioBanks);


struct WVREntityData : WVR_FileEntity {
	const_memory_block	data;
	WVREntityData(const WVR_FileEntity &e, const void *p) : WVR_FileEntity(e), data(p, uint32(e.size)) {}
	WVREntityData(const pair<const WVR_FileEntity &, const void *> &p) : WVR_FileEntity(p.a), data(p.b, uint32(p.a.size)) {}
};

ISO_DEFUSERCOMPV(WVREntityData, type, baseVersion, version, data);

class WEVRFileHandler : public FileHandler {
	const char*		GetExt() override { return "wvr"; }
	const char*		GetDescription() override { return "WEVR file"; }

	int				Check(istream_ref file) override {
		file.seek(0);
		WVR_FileHeader	header;
		return !file.read(header) || !header.valid() ? CHECK_DEFINITE_NO : CHECK_PROBABLE;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		WVR_FileHeader	header;
		if (!file.read(header) || !header.valid())
			return ISO_NULL;

		try {
			WVR_FileContents	info;
			WVR_Reader			reader(file, info, header.version);
			reader.Read();

			ISO_ptr<anything>	p(id);
			ISO_ptr<WVR_MetaData>	m("metadata", info.metadata);
			p->Append(m);
			if (!info.allScenes.empty()) {
				for (auto &i : info.allScenes) {
				#if 0
					ISO_ptr<anything>	p2(i->getName());
					WVR_Renderable		*r	= i->getRoot();
					p2->Append(r->getISO());
					p->Append(p2);
				#else
					p->Append(ISO::MakePtr(i->getName(), *i->pImpl));
				#endif
				}
			} else {
				p->Append(ISO::MakePtr("contents", info));
			}
			return p;

		} catch (WVR_Status err) {
			throw_accum("WVR error" << int(err));
			return ISO_NULL;
		}
	}
#if 0
	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ISO_ptr<mapped_anything>	p(id, fn);
		byte_reader					b(p->m);
		const WVR_FileHeader		*header = b.get_ptr();
		if (!header->valid())
			return ISO_NULL;

		WVR_FileContents	info;
		WVR_Reader			reader(b, info, header->version);

		if (header->version >= 0x00010004) {
			ISO_ptr<WVR_MetaData>	m("metadata");
			reader.doRead(*m);
			p->a.Append(m);
		}

		uint32 itemCount = b.get();

		while (itemCount--) {
			WVR_FileEntity entity;
			b.read(entity);

			uint16 version = entity.version;

			bool doSkip = entity.baseVersion > WVR_Reader::getCurrentBaseVersion();
			if (!doSkip) switch (entity.type) {
				case WVR_TYPE_ID_SCENE:				doSkip = version > WVR_Scene_Impl::cCurrentVersion;				break;
				case WVR_TYPE_ID_SCRIPT:			doSkip = version > WVR_Script_Impl::cCurrentVersion;			break;
				case WVR_TYPE_ID_GROUP:				doSkip = version > WVR_Group_Impl::cCurrentVersion;				break;
				case WVR_TYPE_ID_SWITCHRENDERABLE:	doSkip = version > WVR_SwitchRenderable_Impl::cCurrentVersion;	break;
				case WVR_TYPE_ID_TRIMESH:			doSkip = version > WVR_TriMesh_Impl::cCurrentVersion;			break;
				case WVR_TYPE_ID_PROJSCREEN:		doSkip = version > WVR_ProjScreen_Impl::cCurrentVersion;		break;
				case WVR_TYPE_ID_MOVIE:				doSkip = version > WVR_Movie_Impl::cCurrentVersion;				break;
				case WVR_TYPE_ID_IMAGE:				doSkip = version > WVR_Image_Impl::cCurrentVersion;				break;
				case WVR_TYPE_ID_AUDIOBANK:			doSkip = version > WVR_AudioBank_Impl::cCurrentVersion;			break;
				case WVR_TYPE_ID_TEXTUREHOTSPOTS:	doSkip = version > WVR_TextureHotspots_Impl::cCurrentVersion;	break;
				case WVR_TYPE_ID_FILE:
				default:
					// TODO: currently we do not support nested files
					doSkip = true;
					break;
			}

			if (doSkip) {
				if (entity.size > 0) {
					b = b.p + entity.size;
					continue;
				}
			}

			p->a.Append(ISO_ptr<WVREntityData>(0, make_pair(entity, b.p)));
			b = b.p + entity.size;
		}

		return p;
	}
#endif

} wevr;

