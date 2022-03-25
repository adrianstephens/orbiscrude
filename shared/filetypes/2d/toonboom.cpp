#include "iso/iso_files.h"
#include "comms/zlib_stream.h"
#include "extra/xml.h"
#include "extra/identifier.h"
#include "base/vector.h"
#include "base/algorithm.h"
#include "bitmap/bitmap.h"
#include "vector_iso.h"
#include "hashes/fnv.h"
#include "directory.h"

using namespace iso;

template<typename C, typename F> void clear_from(C *c, F C::*o) {
	memset(&c->*o, 0, (char*)(c + 1) - (char*)&(c->*o));
}

template<typename C, typename F> void clear_from(C *c, F *f) {
	memset(f, 0, (char*)(c + 1) - (char*)f);
}

//-----------------------------------------------------------------------------
//	ToonBoom native/undocumented format
//-----------------------------------------------------------------------------

struct ToonBoomHeader {
	uint32be	tag;	//OTVG
	uint32		id;		//full
	uint32		ver;	//0x3f1
	uint32		n1, n2;	//2, 1

	bool	valid() const { return tag == 'OTVG'; }
};

struct ToonBoomBlock {
	uint32be	tag;
	uint32		len;
};

struct ToonBoomBlock2 {
	uint32be	tag;
	uint32be	comp;
	uint32		len;
};

class ToonBoomFileHandler : public FileHandler {
	const char*		GetExt() override { return "tvg"; }
	const char*		GetDescription() override { return "ToonBoom Vector Graphics"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		return file.get<ToonBoomHeader>().valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override;
} toonboom;

ISO_ptr<void> GetToonBoomBlock(tag id, istream_ref file) {
	ISO_ptr<anything>	p(id);
	ToonBoomBlock2		b;
	char	name[5] = {0};
	while (file.read(b)) {
		auto	end = file.tell() + b.len;
		raw_copy(b.tag, name);

		switch (b.comp) {
			default:
			case 'UNCO': {
				ISO_ptr<ISO_openarray<uint8>>	b2(name, b.len);
				file.readbuff(b2->begin(), b.len);
				p->Append(b2);
				break;
			}
			case 'ZLIB': {
				auto	len	= file.get<uint32>();
				ISO_ptr<ISO_openarray<uint8>>	b2(name, len);
				zlib_reader(file).readbuff(b2->begin(), b.len);
				p->Append(b2);
				break;
			}
		}
		file.seek(end);
	}
	return p;
}

ISO_ptr<void> ToonBoomFileHandler::Read(tag id, istream_ref file) {
	ToonBoomHeader	h;
	if (!file.read(h) || !h.valid())
		return ISO_NULL;

	ISO_ptr<anything>	p(id);
	ToonBoomBlock	b;

	char	name[5] = {0};
	while (file.read(b)) {
		raw_copy(b.tag, name);

		switch (b.tag) {
			case 'TTOC': {
				ISO_ptr<ISO_openarray<ISO_ptr<uint32>>>	b2(name);
				for (int i = 0, n = b.len; i < n; i++) {
					file.read(b);
					raw_copy(b.tag, name);
					b2->Append(ISO_ptr<uint32>(name, b.len));
				}
				file.read(b);
				p->Append(b2);
				break;
			}
			case 'SIGN': {
				ISO_ptr<ISO_openarray<uint8>>	b2(name, 0x46);
				file.readbuff(b2->begin(), 0x46);
				p->Append(b2);
				break;
			}
			case 'UNCO': {//uncompressed?
				p->Append(GetToonBoomBlock(name, lvalue(istream_offset(file, b.len))));
				break;
			}

			default: {
				ISO_ptr<ISO_openarray<uint8>>	b2(name, b.len);
				file.readbuff(b2->begin(), b.len);
				p->Append(b2);
				break;
			}
		}
	}
	return p;
}

//-----------------------------------------------------------------------------
//	ToonBoom xstage
//-----------------------------------------------------------------------------

//	elements
//		elements
//			element
//				drawings
//					dwg
//				layers
//					layer
//	options
//	timelineMarkers
//	scenes
//		scene
//			columns
//				column
//					elementSeq
//					step
//					points
//			options
//				defaultDisplay
//			rootgroup
//				options
//					collapsed
//				nodeslist
//					module
//						options
//						attrs
//					group
//						options
//						attrs
//						nodeslist
//							module
//								options
//								ports
//									port
//								attrs
//						linkedlist
//							link
//				linkedlist
//					link
//			metas
//				meta
//					guideList
//	symbols
//		folder
//			scene
//	timeline
//		scene

// group is tree of modules

/* module types
READ
WRITE
DISPLAY
MULTIPORT_IN
MULTIPORT_OUT
COMPOSITE
PEG
CUTTER	(has attr inverted)
UNDERLAY
LINE_ART
COLOR_ART
KinematicOutputModule
AutoPatchModule
OffsetModule
CurveModule
TransformationSwitch
BendyBoneModule

SimpleNodeTree::eRead
SimpleNodeTree::eCutter
SimpleNodeTree::eInverseCutter
SimpleNodeTree::eMatte
SimpleNodeTree::eDeformation
SimpleNodeTree::eDeformationRoot
SimpleNodeTree::ePeg


*/


dynamic_array<range<int_iterator<int>>> ReadFrames(string_scan &&s) {
	dynamic_array<range<int_iterator<int>>>	frames;

	while (s.remaining()) {
		int	start = s.get();
		if (s.check('-')) {
			int	end = s.get();
			frames.emplace_back(start - 1, end);
		} else {
			frames.emplace_back(start - 1, start);
		}
		if (!s.check(','))
			break;
	}

	return frames;
}


struct ToonBoomNative {

	struct Column {
		enum TYPE {
			TYPE0,		//	<elementSeq exposures="1-22" val="2" id="4"/>
			TYPE1,		//-
			TYPE2,		// path3D/points
			TYPE3,		// step, points
		};
		int		type;
		Column(TYPE type) : type(type) {}
	};

	typedef hash_map_with_key<string, Column*>	Columns;

	struct Column0 : Column {
		dynamic_array<float2p>	pts;
//		dynamic_bitarray<uint64>	enabled;
		Column0(XMLiterator &it) : Column(TYPE0) {
			dynamic_array<float3p>	pts0;
			for (it.Enter(); it.Next();) {
				if (it.data.Is("elementSeq")) {
					float	y = it.data["val"];
					for (auto &i : ReadFrames(string_scan(it.data["exposures"])))
						pts0.push_back() = float3{i.front(), i.back(), y};
				}
			}
			sort(pts0, [](const float3p &a, const float3p &b) { return a.x < b.x;});

			float2p	back = zero;
			for (auto i = pts0.begin(), e = pts0.end(); i != e; ++i) {
				if (back.x != i->x || back.y != i->z) {
					pts.push_back(back);
					if (back.x != i->x) {
						back.y = 0;
						pts.push_back(back);
					}
				}
				back = float2{i->y, i->z};
			}
			pts.push_back(back);
		}
	};
	struct Column2 : Column {
		dynamic_array<float4p>	pts;
		Column2(XMLiterator &it) : Column(TYPE2) {
			for (it.Enter(); it.Next();) {
				if (it.data.Is("path3D")) {
					for (it.Enter(); it.Next();) {
						if (it.data.Is("points")) {
							for (it.Enter(); it.Next();) {
								float	x, y, z;
								string_scan(it.data["val"]) >> x >> ',' >> y >> ',' >> z;
								pts.push_back(float4{x, y, z, (int)it.data["lockedInTime"] - 1});
							}
						}
					}
				}
			}
		}
	};

	struct Column3 : Column {
		dynamic_array<float2p>	pts;
		Column3(XMLiterator &it) : Column(TYPE3) {
			for (it.Enter(); it.Next();) {
				if (it.data.Is("points")) {
					for (it.Enter(); it.Next();) {
						float	y = it.data["y"];
						for (auto &i : ReadFrames(string_scan(it.data["x"]))) {
							pts.push_back(float2{i.front(), y});
							if (i.size() > 1)
								pts.push_back(float2{i.back(), y});
						}
					}
				}
			}
			sort(pts, [](const float2p &a, const float2p &b) { return a.x < b.x;});
		}
	};

	struct Field {
		float	val;
		Column*	col;
		Field() : val(0), col(0) {}
		void Read(XMLiterator &it, const Columns &columns) {
			val	= it.data["val"];
			if (!col)
				col	= columns[it.data["col"]].or_default();
		}
	};

	struct Attribute {
		enum {
			MULTI		= 1 << 0,
			SEPARATE	= 1 << 1,
			INFIELDS	= 1 << 2,
		};
		uint32	flags;
		Attribute() : flags(0) {}
	};

	struct Attribute1 : Attribute, Field {
		Attribute1(XMLiterator &it, const Columns &columns) {
			Read(it, columns);
		}
	};

	struct Attribute3 : Attribute {
		Field	x, y, z;
		Attribute3(XMLiterator &it, const Columns &columns) {
			flags = MULTI;
			for (it.Enter(); it.Next();) {
				if (it.data.Is("separate"))
					flags |= it.data.Get("val", false) ? SEPARATE : 0;
				else if (it.data.Is("inFields"))
					flags |= it.data.Get("val", false) ? INFIELDS : 0;
				else if (it.data.Is("x") || it.data.Is("anglex") || it.data.Is("xy") || it.data.Is("attr3dpath"))
					x.Read(it, columns);
				else if (it.data.Is("y") || it.data.Is("angley"))
					y.Read(it, columns);
				else if (it.data.Is("z") || it.data.Is("anglez"))
					z.Read(it, columns);
			}
		}
	};

	typedef hash_map_with_key<string, Attribute*> _Attributes;
	struct Attributes : _Attributes {
		void Read(XMLiterator &it, const Columns &columns) {
			for (it.Enter(); it.Next();) {
//				ISO_ASSERT(it.data.Name() != "compositeMode");
				if (it.IsBeginEnd()) {
					(*this)[it.data.Name()] = new Attribute1(it, columns);
				} else {
					(*this)[it.data.Name()] = new Attribute3(it, columns);
				}
			}
		}
	};

	struct Sprite {
		ISO_ptr<bitmap>	bm;
		float2x3	matrix;

		Sprite(const filename &fn) {
			bm.CreateExternal(fn);

			FileInput	file(filename(fn) + ".sprite");
			XMLreader	xml(file);
			XMLreader::Data	data;
			if (xml.ReadNext(data) == XMLreader::TAG_BEGINEND) {
				float pivotX = data["pivotX"], pivotY = data["pivotY"], scaleX = data["scaleX"], scaleY = data["scaleY"], angleDegrees = data["angleDegrees"];
				matrix	= rotate2D(degrees(-angleDegrees)) * scale(scaleX, scaleY) * translate(-pivotX, -pivotY);
			}
		}
	};
	struct Layer {
		string	name;
		Layer(const char *name) : name(name) {}
	};

	struct Element {
		string					name;
		anything				drawings;
		dynamic_array<Layer>	layers;

		Element(XMLiterator &it) : name(it.data.Find("elementName")) {
			string	id		= it.data["id"];

			filename	folder	= filename((const char*)it.data["rootFolder"]).add_dir((const char*)it.data["elementFolder"]);
			filename	textures = ISO::FileHandler::FindAbsolute("textures");

			for (it.Enter(); it.Next();) {
				if (it.data.Is("drawings")) {
					for (it.Enter(); it.Next();) {
						if (it.data.Is("dwg")) {
							auto dwg_name = it.data.Find("name");
							if (auto i = directory_recurse(textures, id + "-" + name + "-" + dwg_name + ".png")) {
								drawings.push_back(ISO_ptr<Sprite>(0, i));
							} else {
								drawings.push_back(ISO::MakePtrExternal<void>(ISO::FileHandler::FindAbsolute(folder + "-" + dwg_name + ".tvg")));
							}

							//drawings.emplace_back(fn);
						}
					}
				} else if (it.data.Is("layers")) {
					for (it.Enter(); it.Next();) {
						if (it.data.Is("layer"))
							layers.emplace_back(it.data.Find("name"));
					}
				}
			}
		}
	};

	struct Node : hierarchy<Node> {
		string				name;
		sparse_array<Node*>	inports, outports;
		Attributes			attr;
		Node(XMLiterator &it) : name(it.data.Find("name")) {}
	};

	struct Module : Node {
		Module(XMLiterator &it, Columns &columns) : Node(it) {
			for (it.Enter(); it.Next();) {
				if (it.data.Is("attrs"))
					attr.Read(it, columns);
			}
		}
	};

	struct Group : Node {
		Node*	find(const char *name) {
			for (auto &i : depth_first())
				if (i.name == name)
					return &i;
			return 0;
		}

		Group(XMLiterator &it, Columns &columns) : Node(it) {
			for (it.Enter(); it.Next();) {
				if (it.data.Is("attrs")) {
					attr.Read(it, columns);

				} else if (it.data.Is("nodeslist")) {
					for (it.Enter(); it.Next();) {
						if (it.data.Is("module"))
							attach(new Module(it, columns));
						else if (it.data.Is("group"))
							attach(new Group(it, columns));
					}

				} else if (it.data.Is("linkedlist")) {
					for (it.Enter(); it.Next();) {
						if (it.data.Is("link")) {
							auto	in	= find(it.data["in"]);
							auto	out = find(it.data["out"]);
							if (auto p = it.data["inport"])
								in->inports[p] = out;
							else
								in->attach(out->detach());
							if (auto p = it.data["outport"])
								out->outports[p] = in;
						}
					}
				}
			}
		}
	};


	struct Scene {
		string	name, id;
		Group	*root;

		Scene(XMLiterator &it) : name(it.data.Find("name")), id(it.data.Find("id")) {
			Columns	columns;

			for (it.Enter(); it.Next();) {
				if (it.data.Is("columns")) {
					for (it.Enter(); it.Next();) {
						if (it.data.Is("column")) {
							string	name	= it.data["name"];
							switch ((int)it.data["type"]) {
								case 0:	columns[name] = new Column0(it); break;
								case 2:	columns[name] = new Column2(it); break;
								case 3:	columns[name] = new Column3(it); break;
							}
						}
					}
				} else if (it.data.Is("rootgroup")) {
					root = new Group(it, columns);
				}
			}
		}

	};

	dynamic_array<Element>	elements;
	dynamic_array<Scene>	scenes;

};

ISO_DEFUSERCOMPV(ToonBoomNative::Layer, name);
ISO_DEFUSERCOMPV(ToonBoomNative::Sprite, bm, matrix);
ISO_DEFUSERCOMPV(ToonBoomNative::Element, name, drawings, layers);

ISO_DEFUSERCOMPV(ToonBoomNative::Field, val, col);
ISO_DEFUSERCOMP(ToonBoomNative::Attribute1, 1) { ISO_SETBASE(0, ToonBoomNative::Field);}};
ISO_DEFUSERCOMPV(ToonBoomNative::Attribute3, x, y, z);

ISO_DEFUSER(ToonBoomNative::Attributes, ToonBoomNative::_Attributes);

ISO_DEFUSERCOMPV(ToonBoomNative::Column0, pts);
ISO_DEFUSERCOMPV(ToonBoomNative::Column2, pts);
ISO_DEFUSERCOMPV(ToonBoomNative::Column3, pts);

template<> struct ISO::def<ToonBoomNative::Column*> : ISO::VirtualT2<ToonBoomNative::Column*> {
	ISO::Browser2	Deref(ToonBoomNative::Column *col) {
		if (col) switch (col->type) {
			case 0:	return MakeBrowser(*(ToonBoomNative::Column0*)col);
			case 2:	return MakeBrowser(*(ToonBoomNative::Column2*)col);
			case 3:	return MakeBrowser(*(ToonBoomNative::Column3*)col);
		}
		return ISO::Browser2();
	}
};

template<> struct ISO::def<ToonBoomNative::Attribute> : ISO::VirtualT2<ToonBoomNative::Attribute> {
	ISO::Browser2	Deref(ToonBoomNative::Attribute &a) {
		return a.flags & ToonBoomNative::Attribute::MULTI
			? MakeBrowser(*(ToonBoomNative::Attribute3*)&a)
			: MakeBrowser(*(ToonBoomNative::Attribute1*)&a);
	}
};

ISO_DEFUSERCOMPV(ToonBoomNative::Node, name, children, inports, outports, attr);
ISO_DEFUSER(ToonBoomNative::Group, ToonBoomNative::Node);
ISO_DEFUSERCOMPV(ToonBoomNative::Scene, name, id, root);

ISO_DEFUSERCOMPV(ToonBoomNative, elements, scenes);


class ToonBoomXStageFileHandler : public FileHandler {
	const char*		GetExt() override { return "xstage"; }
	const char*		GetDescription() override { return "ToonBoom stage"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		XMLreader				xml(file);
		XMLreader::Data	data;
		return xml.CheckVersion() == 1.0f && xml.ReadNext(data, XMLreader::TAG_BEGIN) == XMLreader::TAG_BEGIN && data.Is("project") && data["creator"] == "harmony"
			? CHECK_PROBABLE
			: CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		XMLreader				xml(file);
		XMLreader::Data	data;

		ISO_ptr<ToonBoomNative>	tb(id);

		for (XMLiterator it(xml, data); it.Next();) {
			if (data.Is("project")) {
				for (it.Enter(); it.Next();) {
					switch (string_hash(data.Name())) {
						case "elements"_fnv:
							for (it.Enter(); it.Next();) {
								if (it.data.Is("element"))
									tb->elements.emplace_back(it);
							}
							break;
						case "scenes"_fnv:
							for (it.Enter(); it.Next();) {
								if (it.data.Is("scene"))
									tb->scenes.emplace_back(it);
							}
							break;
					}
//					if (data.Is("elements"))
//						ReadElements(it);
//
//					else if (data.Is("scenes"))
//						ReadScenes(it);
				}
			}
		}
		return tb;
	}
} toonboom_xstage;

//-----------------------------------------------------------------------------
//	ToonBoom exported XML
//-----------------------------------------------------------------------------

enum DataObjectId_t {
	eNullData = 0,
	eNodeData,
	eChannelData,
	eBezierCurveData,
	eBezierPointData,
	eCatmullCurveData,
	eCatmullPointData,
	eLinearCurveData,
	eLinearPointData,
	eAnimatedPivotData,
	ePivotPointData,
	eDrawingAnimationData,
	eDrawingData,
	eFloatData,
	eStringData,
	eEffectData,

	MAX_IDS
};

enum CurveChannel_t {
	eNullChannel = 0,

	eSeparateX,
	eSeparateY,
	eSeparateZ,
	eXYZ,
	eVelocity,
	eScaleX,
	eScaleY,
	eScaleXY,
	eRotationZ,
	eSkew,
	ePivot,

	eRestOffsetX,
	eRestOffsetY,
	eRestLength,
	eRestRotation,

	eDeformOffsetX,
	eDeformOffsetY,
	eDeformLength,
	eDeformRotation,

	eOpacity,

	MAX_CURVE_CHANNELS
};

enum EffectId_t {
	eNoop		   = 0,
	eCutter		   = 1 << 0,
	eInverseCutter = 1 << 1,
	eDeformation   = 1 << 2,

	eAllCutters = eCutter | eInverseCutter
};

static const int g_nullOffset = -1;

struct DataOffset_t {
	int	offset;
	explicit DataOffset_t(int offset)	: offset(offset) {}
	DataOffset_t()						: offset(g_nullOffset) {}
	DataOffset_t(const nullptr_t&)		: offset(g_nullOffset) {}
	template<typename T, typename B> DataOffset_t(T *p, B *base) : offset((uint8*)p - (uint8*)base) {}
	explicit operator bool()		const { return offset != g_nullOffset; }
	bool operator!()				const { return offset == g_nullOffset; }
	bool operator<(DataOffset_t b)	const { return offset < b.offset; }
	bool operator==(DataOffset_t b)	const { return offset == b.offset; }

	friend DataOffset_t operator+(DataOffset_t ref, DataOffset_t off) { return DataOffset_t(ref.offset + off.offset); }
	friend DataOffset_t operator-(DataOffset_t ref, DataOffset_t off) { return DataOffset_t(ref.offset - off.offset); }
};
typedef DataOffset_t DataRef_t;

template<> struct field_names<CurveChannel_t>	{ static field_value s[]; };
field_value field_names<CurveChannel_t>::s[] = {
	"position.x",				eSeparateX,
	"offset.x",					eSeparateX,
	"position.y",				eSeparateY,
	"offset.y",					eSeparateY,
	"position.z",				eSeparateZ,
	"offset.z",					eSeparateZ,

	"position.attr3dpath",		eXYZ,
	"offset.attr3dpath",		eXYZ,
	"offset",					eXYZ,

	"velocity",					eVelocity,
	"scale.x",					eScaleX,
	"scale.y",					eScaleY,
	"scale.xy",					eScaleXY,
	"rotation.anglez",			eRotationZ,
	"rotation.z",				eRotationZ,
	"skew",						eSkew,
	"pivot",					ePivot,

	"rest.offset.x",			eRestOffsetX,
	"rest.offset.y",			eRestOffsetY,
	"rest.length",				eRestLength,
	"rest.rotation.anglez",		eRestRotation,
	"rest.rotation.z",			eRestRotation,

	"deform.offset.x",			eDeformOffsetX,
	"deform.offset.y",			eDeformOffsetY,
	"deform.length",			eDeformLength,
	"deform.rotation.anglez",	eDeformRotation,
	"deform.rotation.z",		eDeformRotation,

	"opacity",					eOpacity,
};

struct TR_DataObject {
	DataObjectId_t id;
	TR_DataObject(DataObjectId_t id) : id(id) {}
	TR_DataObject *get(DataOffset_t offset) const {
		return offset ? (TR_DataObject*)((uint8*)this + offset.offset) : nullptr;
	}
	template<typename T> T *get(DataOffset_t offset) const {
		TR_DataObject *d = get(offset);
		ISO_ASSERT(!d || d->id == T::Id());
		return static_cast<T*>(d);
	}
};

template<DataObjectId_t ID> struct TR_DataObjectT : TR_DataObject {
	TR_DataObjectT() : TR_DataObject(ID) {}
	static constexpr DataObjectId_t	Id() { return ID; }
};

struct TR_StringDataObject : TR_DataObjectT<eStringData> {
	unsigned		nChars;
	auto			get()		const { return count_string((const char*)(this + 1), nChars); }
	auto			gets()		const { return (const char*)(this + 1); }
	TR_StringDataObject(unsigned nChars, const char *s) : nChars(nChars) { memcpy(this + 1, s, nChars + 1); }
};

struct TR_EffectDataObject : TR_DataObjectT<eEffectData> {
	EffectId_t		effectId;
	DataOffset_t	matteDataOffset;
	auto			matte()		const;
	TR_EffectDataObject(EffectId_t effectId) : effectId(effectId) {}
};

struct TR_ChannelDataObject : TR_DataObjectT<eChannelData> {
	CurveChannel_t	channelType;
	DataOffset_t	linkedDataOffset;
	DataOffset_t	nextChannelDataOffset;
	auto			linked()		const { return get(linkedDataOffset); }
	auto			nextChannel()	const { return get<TR_ChannelDataObject>(nextChannelDataOffset); }
	TR_ChannelDataObject(CurveChannel_t channelType) : channelType(channelType) {}
};

struct TR_DrawingDataObject : TR_DataObjectT<eDrawingData> {
	float			frame;
	float			repeat;
	DataOffset_t	drawingNameOffset;

	cstring			name() const {
		if (auto stringData = get<TR_StringDataObject>(drawingNameOffset))
			return stringData->gets();
		return 0;
	}
	TR_DrawingDataObject(float frame, float repeat) : frame(frame), repeat(repeat) {}
};
struct TR_DrawingAnimationDataObject : TR_DataObjectT<eDrawingAnimationData> {
	unsigned		nDrawings	= 0;
	auto			entries()	const { return make_range_n((TR_DrawingDataObject*)(this + 1), nDrawings); }
};

struct TR_NodeDataObject : TR_DataObjectT<eNodeData> {
	DataOffset_t	nameOffset;
	DataOffset_t	channelDataOffset;
	DataOffset_t	drawingDataOffset;
	DataOffset_t	effectDataOffset;
	DataOffset_t	brotherDataOffset;
	DataOffset_t	childDataOffset;

	cstring			name() const {
		if (auto stringData = get<TR_StringDataObject>(nameOffset))
			return stringData->gets();
		return 0;
	}
	auto			channel()	const { return get<TR_ChannelDataObject>(channelDataOffset); }
	auto			drawing()	const { return get<TR_DrawingAnimationDataObject>(drawingDataOffset); }
	auto			effect()	const { return get<TR_EffectDataObject>(effectDataOffset); }
	auto			brother()	const { return get<TR_NodeDataObject>(brotherDataOffset); }
	auto			child()		const { return get<TR_NodeDataObject>(childDataOffset); }
};

auto TR_EffectDataObject::matte() const { return get<TR_NodeDataObject>(matteDataOffset); }

struct TR_BezierPointDataObject : TR_DataObjectT<eBezierPointData> {
	typedef struct TR_BezierCurveDataObject holder_t;
	float2p			v, left, right;
	bool			constSeg;
	TR_BezierPointDataObject() { clear_from(this, &v); }
};
struct TR_BezierCurveDataObject : TR_DataObjectT<eBezierCurveData> {
	unsigned		nPoints	= 0;
	auto			points()	const { return make_range_n((TR_BezierPointDataObject*)(this + 1), nPoints); }
};

struct TR_CatmullPointDataObject : TR_DataObjectT<eCatmullPointData> {
	typedef struct TR_CatmullCurveDataObject holder_t;
	float			frame;
	float3p			v;
	float			tension, continuity, bias, distance;
	float3p			d_in, d_out;
	TR_CatmullPointDataObject() : frame(-1) { clear_from(this, &v); }
};
struct TR_CatmullCurveDataObject : TR_DataObjectT<eCatmullCurveData> {
	float3p			scale;
	unsigned		nPoints;
	auto			points()	const { return make_range_n((TR_CatmullPointDataObject*)(this + 1), nPoints); }
	TR_CatmullCurveDataObject() : scale((float3)one), nPoints(0) {}
};

struct TR_LinearPointDataObject : TR_DataObjectT<eLinearPointData> {
	typedef struct TR_LinearCurveDataObject holder_t;
	float2p			v;
	TR_LinearPointDataObject() { clear(v); }
};
struct TR_LinearCurveDataObject : TR_DataObjectT<eLinearCurveData> {
	unsigned		nPoints	= 0;
	auto			points()	const { return make_range_n((TR_LinearPointDataObject*)(this + 1), nPoints); }
};

struct TR_PivotPointDataObject : TR_DataObjectT<ePivotPointData> {
	typedef struct TR_AnimatedPivotDataObject holder_t;
	float			frame;
	float3p			v;
	TR_PivotPointDataObject() : frame(-1) { clear(v); }
};
struct TR_AnimatedPivotDataObject : TR_DataObjectT<eAnimatedPivotData> {
	unsigned		nPoints	= 0;
	auto			points()	const { return make_range_n((TR_PivotPointDataObject*)(this + 1), nPoints); }
};

struct TR_FloatDataObject : TR_DataObjectT<eFloatData> {
	float			value;
};

static const int	NB_PTS_PER_SEG	= 50;
static const double	FLOAT_EPSILON	= 0.0000001;

struct Discreet {
	double x, y, z;
	double distance;
	Discreet() : x(0), y(0), z(0), distance(0) {}
};

void getValueU(double u, const TR_CatmullPointDataObject *p1, const TR_CatmullPointDataObject *p2, double &x, double &y, double &z) {
	x =	(((p2->d_in.x +     p1->d_out.x - 2 * p2->v.x + 2 * p1->v.x) * u
		+ -p2->d_in.x - 2 * p1->d_out.x + 3 * p2->v.x - 3 * p1->v.x) * u
		+  p1->d_out.x) * u
		+  p1->v.x;

	y =	(((p2->d_in.y +     p1->d_out.y - 2 * p2->v.y + 2 * p1->v.y) * u
		+ -p2->d_in.y - 2 * p1->d_out.y + 3 * p2->v.y - 3 * p1->v.y) * u
		+  p1->d_out.y) * u
		+  p1->v.y;

	z =	(((p2->d_in.z +     p1->d_out.z - 2 * p2->v.z + 2 * p1->v.z) * u
		+ -p2->d_in.z - 2 * p1->d_out.z + 3 * p2->v.z - 3 * p1->v.z) * u
		+  p1->d_out.z) * u
		+  p1->v.z;
}

void getDiscreetValue(const TR_CatmullPointDataObject *p1, const TR_CatmullPointDataObject *p2, float wanted_distance, Discreet *point) {
	int			mmax = NB_PTS_PER_SEG;
	double		du	= 1 / double(mmax);
	Discreet	pa, pb; // start and end of each small segment during interpolation

	pa.x = p1->v.x;  // prepare for first segment
	pa.y = p1->v.y;
	pa.z = p1->v.z;
	pa.distance = p1->distance;

	for (int i = 1; i <= mmax; ++i) {
		getValueU(i * du, p1, p2, pb.x, pb.y, pb.z);

		double	dx = pb.x - pa.x;
		double	dy = pb.y - pa.y;
		double	dz = pb.z - pa.z;

		dy = dy * 0.75;
		dz = dz * 2.0;
		pb.distance = pa.distance + (dx == 0 && dy == 0 && dz == 0 ? FLOAT_EPSILON : sqrt(dx * dx + dy * dy + dz * dz));

		if (point && wanted_distance <= pb.distance)
			break;

		pa = pb;
	}

	if (point) {
		double t = (wanted_distance - pa.distance) / (pb.distance - pa.distance);
		point->x = lerp(pa.x, pb.x, t);
		point->y = lerp(pa.y, pb.y, t);
		point->z = lerp(pa.z, pb.z, t);
	}
}

double	getDistance(const TR_CatmullPointDataObject *p1, const TR_CatmullPointDataObject *p2) {
	int			mmax = NB_PTS_PER_SEG;
	double		du	= 1 / double(mmax);
	Discreet	pa, pb; // start and end of each small segment during interpolation

	pa.x = p1->v.x;  // prepare for first segment
	pa.y = p1->v.y;
	pa.z = p1->v.z;
	pa.distance = 0;

	for (int i = 1; i <= mmax; ++i) {
		getValueU(i * du, p1, p2, pb.x, pb.y, pb.z);

		double	dx = pb.x - pa.x;
		double	dy = pb.y - pa.y;
		double	dz = pb.z - pa.z;

		dy = dy * 0.75;
		dz = dz * 2.0;
		pb.distance = pa.distance + (dx == 0 && dy == 0 && dz == 0 ? FLOAT_EPSILON : sqrt(dx * dx + dy * dy + dz * dz));

		pa = pb;
	}

	return pb.distance;
}

void computeCatmullDistances(const range<TR_CatmullPointDataObject*> &points) {
	double totalDistance = 0;
	for (auto &p : points.slice(1)) {
		totalDistance += getDistance(&p - 1, &p);
		p.distance = (float)totalDistance;
	}
}

void computeCatmullDerivatives(const range<TR_CatmullPointDataObject*> &points) {
	for (auto &p : points.slice(1)) {
		auto	p0 = &p;
		auto	p1 = p0 + 1 == points.end() ? p0 : p0 + 1;
		auto	p2 = p1 + 1 == points.end() ? p1 : p1 + 1;

		double	dist1x = (double)p1->v.x - (double)p0->v.x;
		double	dist1y = (double)p1->v.y - (double)p0->v.y;
		double	dist1z = (double)p1->v.z - (double)p0->v.z;

		double	dist2x = (double)p2->v.x - (double)p1->v.x;
		double	dist2y = (double)p2->v.y - (double)p1->v.y;
		double	dist2z = (double)p2->v.z - (double)p1->v.z;

		dist1y = dist1y * 0.75;
		dist1z = dist1z * 2.0;
		dist2y = dist2y * 0.75;
		dist2z = dist2z * 2.0;

		double	a1 = sqrt(dist1x * dist1x + dist1y * dist1y + dist1z * dist1z);
		double	a2 = sqrt(dist2x * dist2x + dist2y * dist2y + dist2z * dist2z);
		double	denom = a2 + a1;

		if (denom == 0) {
			p1->d_in.x = p1->d_in.y = p1->d_in.z = 0;
			p1->d_out.x = p1->d_out.y = p1->d_out.z = 0;

		} else {
			double t = p1->tension;
			double c = p1->continuity;
			double b = p1->bias;

			//Ref: equation 8 and 9 of Kochanek paper with in-house modification to avoid the loop problem when 2 points are too close to each other.
			p1->d_in.x	= (float)(((1 - t) * (1 - c) * (1 + b) * (dist1x * a2) + (1 - t) * (1 + c) * (1 - b) * (dist2x * a1)) / denom);
			p1->d_in.y	= (float)(((1 - t) * (1 - c) * (1 + b) * (dist1y * a2) + (1 - t) * (1 + c) * (1 - b) * (dist2y * a1)) / denom);
			p1->d_in.z	= (float)(((1 - t) * (1 - c) * (1 + b) * (dist1z * a2) + (1 - t) * (1 + c) * (1 - b) * (dist2z * a1)) / denom);

			p1->d_out.x = (float)(((1 - t) * (1 + c) * (1 + b) * (dist1x * a2) + (1 - t) * (1 - c) * (1 - b) * (dist2x * a1)) / denom);
			p1->d_out.y = (float)(((1 - t) * (1 + c) * (1 + b) * (dist1y * a2) + (1 - t) * (1 - c) * (1 - b) * (dist2y * a1)) / denom);
			p1->d_out.z = (float)(((1 - t) * (1 + c) * (1 + b) * (dist1z * a2) + (1 - t) * (1 - c) * (1 - b) * (dist2z * a1)) / denom);

			p1->d_in.y	= p1->d_in.y / 0.75f;
			p1->d_in.z	= p1->d_in.z / 2.0f;
			p1->d_out.y	= p1->d_out.y / 0.75f;
			p1->d_out.z = p1->d_out.z / 2.0f;
		}
		++p0;
	}

	//Special case: first and last control point. We will compute a derivative based on a second degree curve instead of a third degree curve.

	if (points.size() > 1) {
		//  first and last point
		auto	*p = points.begin();
		p[0].d_in.x		= p[0].d_in.y = p[0].d_in.z = 0;
		p[0].d_out.x	= -p[1].d_in.x + 2 * p[1].v.x - 2 * p[0].v.x;
		p[0].d_out.y	= -p[1].d_in.y + 2 * p[1].v.y - 2 * p[0].v.y;
		p[0].d_out.z	= -p[1].d_in.z + 2 * p[1].v.z - 2 * p[0].v.z;

		//  2 last points
		p = points.end() - 2;
		p[1].d_in.x		= -p[0].d_out.x + 2 * p[1].v.x - 2 * p[0].v.x;
		p[1].d_in.y		= -p[0].d_out.y + 2 * p[1].v.y - 2 * p[0].v.y;
		p[1].d_in.z		= -p[0].d_out.z + 2 * p[1].v.z - 2 * p[0].v.z;
		p[1].d_out.x	= p[1].d_out.y = p[1].d_out.z = 0;

	} else if (points.size() > 0) {
		// function has a single control point - reset all values. set some default value to first point.
		auto	*p = points.begin();
		p[0].d_in.x		= p[0].d_in.y = p[0].d_in.z = 0;
		p[0].d_in.x		= p[0].d_in.y = p[0].d_in.z = 0;
	}
}
//-----------------------------------------------------------------------------
//	TR_NodeTree
//-----------------------------------------------------------------------------

class TR_NodeTree {
	enum { BUFFER_CHUNK_SIZE = 0x200 };
public:
	typedef set<DataRef_t> DataRefCol_t;

	malloc_block	datablock;
	size_t			size;
	string			spriteSheetName;

	void* alloc(size_t n) {
		if (size + n >= datablock.length())
			datablock.resize(size ? size << 1 : BUFFER_CHUNK_SIZE);
		auto	offset = size;
		size	+= n;
		return datablock + offset;
	}
	   	TR_DataObject*		dataObject(DataRef_t dataRef) {
		return !dataRef || dataRef.offset >= size ? 0 : (TR_DataObject*)(datablock + dataRef.offset);
	}
	template<typename T> T*	dataObject(DataRef_t dataRef) {
		TR_DataObject* data = dataObject(dataRef);
		ISO_ASSERT(!data || data->id == T::Id());
		return static_cast<T*>(data);
	}

	const TR_DataObject* dataObject(DataRef_t dataRef) const {
		return dataRef && dataRef.offset < size ? (const TR_DataObject*)(datablock + dataRef.offset) : nullptr;
	}
	template<typename T> const T* dataObject(DataRef_t dataRef) const {
		const TR_DataObject* data = dataObject(dataRef);
		return data && data->id == T::Id() ? (const T*)data : nullptr;
	}

	template<typename T, typename...P> T* addDataObject(P&&...p) {
		return new(alloc(sizeof(T))) T(forward<P>(p)...);
	}

	TR_StringDataObject* addStringDataObject(const char *name) {
		size_t	len	= strlen(name);
		return new(alloc(align(sizeof(TR_StringDataObject) + len + 1, 4))) TR_StringDataObject((unsigned)len, name);
	}

	DataRef_t	dataRef(const TR_DataObject* dataObject, DataOffset_t offset) const {
		return (DataRef_t)((const uint8*)dataObject - datablock + offset.offset);
	}
	DataRef_t	dataRef(const TR_DataObject* dataObject) const {
		return (DataRef_t)((const uint8*)dataObject - datablock);
	}

	class NodeIterator {
		struct IterData {
			TR_NodeDataObject* data;
			bool			   visitedBrother, visitedChild, visitedMatte;
			IterData(TR_NodeDataObject* data) : data(data), visitedBrother(false), visitedChild(false), visitedMatte(false) {}
			bool operator==(const IterData& iterData) const { return data == iterData.data; }
		};
		dynamic_array<IterData>		stack;
	public:
		NodeIterator(TR_NodeDataObject* nodeData)		{ if (nodeData) stack.push_back(nodeData); }
		NodeIterator& operator++();
		NodeIterator  operator++(int)					{ NodeIterator t = *this; operator++(); return t; }
		bool operator==(const NodeIterator& it) const	{ return stack.empty() ? it.stack.empty() : (!it.stack.empty() && stack.back() == it.stack.back()); }
		bool operator!=(const NodeIterator& it) const	{ return !(operator==(it)); }
		TR_NodeDataObject&	operator*()			const	{ return *stack.back().data;}
		TR_NodeDataObject*	operator->()		const	{ return stack.back().data;}
	};


	TR_NodeTree() : size(0) {
		dataRef(addDataObject<TR_NodeDataObject>());
	}

	TR_NodeDataObject*	root() const {
		auto data = (TR_NodeDataObject*)datablock;
		return data->id == eNodeData ? data : 0;
	}
	range<NodeIterator>	nodes() const {
		return {root(), nullptr};
	}

	void		nodeDataRefs(const char *name, DataRefCol_t& dataRefs) const;
	DataRef_t	nodeDataRef(const char *name) const {
		DataRefCol_t dataRefs;
		nodeDataRefs(name, dataRefs);
		return dataRefs.empty() ? nullptr : *dataRefs.begin();
	}

	TR_NodeTree *finalize() {
		datablock.resize(size);
		return this;
	}
};

TR_NodeTree::NodeIterator& TR_NodeTree::NodeIterator::operator++() {
	TR_NodeDataObject	*prev = nullptr;

	while (!stack.empty()) {
		IterData&		   iterData = stack.back();
		TR_NodeDataObject* nodeData = iterData.data;

		//  Go to next child if exists
		if (!iterData.visitedChild) {
			iterData.visitedChild = true;
			if (auto child = nodeData->child())
				stack.push_back(child);
			break;
		}

		//  Go to next matte if exists
		if (!iterData.visitedMatte) {
			iterData.visitedMatte = true;
			if (auto* effect = nodeData->effect()) {
				if (auto matte = effect->matte()) {
					stack.push_back(matte);
					break;
				}
			}
		}

		//  Go to next brother if exists
		if (!iterData.visitedBrother) {
			iterData.visitedBrother = true;
			if (auto brother = nodeData->brother())
				stack.push_back(brother);
			break;
		}

		prev = nodeData;
		stack.pop_back();
	}
	return *this;
}

void TR_NodeTree::nodeDataRefs(const char *name, DataRefCol_t& dataRefs) const {
#if 0
	dataRefs = transformc(
		filter(nodes(), [name](const TR_NodeDataObject &node) { return node.name() == name; }),
		[this](const TR_NodeDataObject &node) { return dataRef(&node); }
	);
#else
	dataRefs.clear();
	for (auto &i : nodes()) {
		if (i.name() == name)
			dataRefs.insert(dataRef(&i));
	}
#endif
}

//-----------------------------------------------------------------------------
//	TR_NodeTreeBuilder
//-----------------------------------------------------------------------------

class TR_NodeTreeBuilder {
protected:
	TR_NodeTree*	tree;
	DataRef_t		block;
	map<DataRef_t, string>	currentSpriteMapping;

	DataRef_t	createChannelData(CurveChannel_t channelType, DataRef_t nodeRef);

public:
	TR_NodeTreeBuilder(TR_NodeTree* tree) : tree(tree) {}

	DataRef_t	addNode(const char *name)	{ return addNode(name, DataRef_t(0)); }
	DataRef_t	addNode(const char *name, DataRef_t parentNodeRef);

	void		linkNodeAnimation(DataRef_t srcNodeRef, DataRef_t dstNodeRef);
	void		linkChannel(DataRef_t srcChannelRef, DataRef_t dstChannelRef);
	void		linkNodeDrawingAnimation(DataRef_t srcNodeRef, DataRef_t dstNodeRef);

	DataRef_t	createEmptyChannel(CurveChannel_t channel, DataRef_t nodeRef);
	bool		createEffect(DataRef_t nodeRef, EffectId_t effectId);
	DataRef_t	addMatteNode(const char *name, DataRef_t nodeRef);

	DataRef_t	addConstantValue(CurveChannel_t channel, DataRef_t nodeRef, float value);

	bool		beginDrawingSequence(DataRef_t nodeRef);
	void		endDrawingSequence();
	void		addDrawing(const char *spriteSheetName, const char *spriteName, float time, float repeat);
};

DataRef_t TR_NodeTreeBuilder::addNode(const char *name, DataRef_t parentNodeRef) {
	//  Do not allow atomic operation during block operations.
	if (block) {
		ISO_TRACE("Cannot perform atomic operation inside block operation.\n");
		return nullptr;
	}

	TR_NodeDataObject*	nodeData	= tree->addDataObject<TR_NodeDataObject>();
	DataRef_t			nodeDataRef = tree->dataRef(nodeData);
	TR_StringDataObject* stringData = tree->addStringDataObject(name);

	//  Retrieve back from memory as it might have changed during malloc.
	nodeData = tree->dataObject<TR_NodeDataObject>(nodeDataRef);
	if (!nodeData) {
		ISO_TRACE("Could not find node data.\n");
		return nullptr;
	}

	nodeData->nameOffset = DataOffset_t(stringData, nodeData);

	//  Add new node information in parent node.
	TR_NodeDataObject* parentNodeData = tree->dataObject<TR_NodeDataObject>(parentNodeRef);
	if (!parentNodeData) {
		ISO_TRACE("Could not find parent node data.\n");
		return nullptr;
	}

	//  If parent node does not have any child, add current node as first child.
	if (!parentNodeData->childDataOffset) {
		parentNodeData->childDataOffset = DataOffset_t(nodeData, parentNodeData);

	} else {
		//  Otherwise, add to brothers of first child.
		//  Retrieve first brother.
		TR_NodeDataObject*	brotherNodeData = parentNodeData->get<TR_NodeDataObject>(parentNodeData->childDataOffset);
		if (!brotherNodeData) {
			ISO_TRACE("Could not find brother node data.\n");
			return nullptr;
		}

		//  Iterate up to last brother.
		while (brotherNodeData->brotherDataOffset) {
			brotherNodeData = brotherNodeData->get<TR_NodeDataObject>(brotherNodeData->brotherDataOffset);
			if (!brotherNodeData) {
				ISO_TRACE("Could not find brother node data.\n");
				return nullptr;
			}
		}

		brotherNodeData->brotherDataOffset = DataOffset_t(nodeData, brotherNodeData);
	}
	return nodeDataRef;
}

void TR_NodeTreeBuilder::linkNodeAnimation(DataRef_t srcNodeRef, DataRef_t dstNodeRef) {
	//  Warning.  This code will replace link to animation data but will not remove any existing animation data that might have been created before hand.
	TR_NodeDataObject* srcNodeData = tree->dataObject<TR_NodeDataObject>(srcNodeRef);
	if (!srcNodeData) {
		ISO_TRACE("No valid source node data.\n");
		return;
	}
	TR_NodeDataObject* dstNodeData = tree->dataObject<TR_NodeDataObject>(dstNodeRef);
	if (!dstNodeData) {
		ISO_TRACE("No valid destination node data.\n");
		return;
	}
	DataRef_t channelDataRef = tree->dataRef(srcNodeData, srcNodeData->channelDataOffset);
	dstNodeData->channelDataOffset = channelDataRef - tree->dataRef(dstNodeData);
}

void TR_NodeTreeBuilder::linkChannel(DataRef_t srcChannelRef, DataRef_t dstChannelRef) {
	TR_ChannelDataObject* srcChannelData = tree->dataObject<TR_ChannelDataObject>(srcChannelRef);
	if (srcChannelData == 0) {
		ISO_TRACE("No valid source channel data.\n");
		return;
	}
	TR_ChannelDataObject* dstChannelData = tree->dataObject<TR_ChannelDataObject>(dstChannelRef);
	if (dstChannelData == 0) {
		ISO_TRACE("No valid destination channel data.\n");
		return;
	}
	DataRef_t functionDataRef = tree->dataRef(srcChannelData, srcChannelData->linkedDataOffset);
	dstChannelData->linkedDataOffset	= functionDataRef - dstChannelRef;
}

void TR_NodeTreeBuilder::linkNodeDrawingAnimation(DataRef_t srcNodeRef, DataRef_t dstNodeRef) {
	//  Warning.  This code will replace link to animation data but will not remove any existing animation data that might have been created before hand.
	TR_NodeDataObject* srcNodeData = tree->dataObject<TR_NodeDataObject>(srcNodeRef);
	if (srcNodeData == 0) {
		ISO_TRACE("No valid source node data.\n");
		return;
	}
	if (!srcNodeData->drawingDataOffset) {
		ISO_TRACE("No drawing animation data to link from.\n");
		return;
	}
	TR_NodeDataObject* dstNodeData = tree->dataObject<TR_NodeDataObject>(dstNodeRef);
	if (dstNodeData == 0) {
		ISO_TRACE("No valid destination node data.\n");
		return;
	}
	if (dstNodeData->drawingDataOffset) {
		ISO_TRACE("Destination already has drawing animation data.\n");
		return;
	}
	DataRef_t drawingDataRef = tree->dataRef(srcNodeData, srcNodeData->drawingDataOffset);
	dstNodeData->drawingDataOffset = drawingDataRef - tree->dataRef(dstNodeData);
}

DataRef_t TR_NodeTreeBuilder::createEmptyChannel(CurveChannel_t channelType, DataRef_t nodeRef) {
	//  Do not allow atomic operation during block operations.
	if (block) {
		ISO_TRACE("Cannot perform atomic operation inside block operation.\n");
		return nullptr;
	}
	//  Create new Channel Data.
	return createChannelData(channelType, nodeRef);
}

bool TR_NodeTreeBuilder::createEffect(DataRef_t nodeRef, EffectId_t effectId) {
	//  Do not allow atomic operation during block operations.
	if (block) {
		ISO_TRACE("Cannot perform atomic operation inside block operation.\n");
		return false;
	}
	TR_NodeDataObject* nodeData = tree->dataObject<TR_NodeDataObject>(nodeRef);
	if (!nodeData)
		return false;

	if (!nodeData->effectDataOffset) {
		TR_EffectDataObject* effectData = tree->addDataObject<TR_EffectDataObject>(effectId);

		//  Retrieve back from memory as it might have changed during malloc.
		nodeData = tree->dataObject<TR_NodeDataObject>(nodeRef);
		if (nodeData)
			nodeData->effectDataOffset = DataOffset_t(effectData, nodeData);
	}
	return true;
}

DataRef_t TR_NodeTreeBuilder::addMatteNode(const char *name, DataRef_t parentNodeRef) {
	//  Do not allow atomic operation during block operations.
	if (block) {
		ISO_TRACE("Cannot perform atomic operation inside block operation.\n");
		return nullptr;
	}
	TR_NodeDataObject*	nodeData	= tree->addDataObject<TR_NodeDataObject>();
	DataRef_t			nodeDataRef	= tree->dataRef(nodeData);
	TR_StringDataObject* stringData	= tree->addStringDataObject(name);

	//  Retrieve back from memory as it might have changed during malloc.
	nodeData = tree->dataObject<TR_NodeDataObject>(nodeDataRef);
	if (!nodeData) {
		ISO_TRACE("Could not find node data.\n");
		return nullptr;
	}
	nodeData->nameOffset = DataOffset_t(stringData, nodeData);

	TR_NodeDataObject*		parentData = tree->dataObject<TR_NodeDataObject>(parentNodeRef);
	TR_EffectDataObject*	effectData = parentData ? parentData->effect() : nullptr;

	if (!effectData) {
		ISO_TRACE("Could not retrieve effect data.\n");
		return nullptr;
	}
	if (effectData->matteDataOffset) {
		ISO_TRACE("Effect already has a matte.\n");
		return nullptr;
	}
	effectData->matteDataOffset  = DataOffset_t(nodeData, effectData);
	return nodeDataRef;
}

DataRef_t TR_NodeTreeBuilder::addConstantValue(CurveChannel_t channel, DataRef_t nodeRef, float value) {
	//  Do not allow nested curve building
	if (block) {
		ISO_TRACE("Cannot perform atomic operation inside block operation.\n");
		return nullptr;
	}
	//  Create new Channel Data.
	DataRef_t channelDataRef = createChannelData(channel, nodeRef);

	//  Add constant float data object to node tree.
	TR_FloatDataObject* constantData = tree->addDataObject<TR_FloatDataObject>();
	constantData->value			 = value;

	TR_ChannelDataObject* channelData	= tree->dataObject<TR_ChannelDataObject>(channelDataRef);
	channelData->linkedDataOffset		= DataOffset_t(constantData, channelData);
	return channelDataRef;
}

bool TR_NodeTreeBuilder::beginDrawingSequence(DataRef_t nodeRef) {
	if (block) {
		ISO_TRACE("Already inside block operation.  Nested block operations are not permitted.\n");
		return false;
	}
	//  Do not allow a new drawing sequence when one has already been set.
	if (TR_NodeDataObject* nodeData = tree->dataObject<TR_NodeDataObject>(nodeRef)) {
		if (!nodeData->drawingDataOffset) {
			TR_DrawingAnimationDataObject* drawingAnimationData = tree->addDataObject<TR_DrawingAnimationDataObject>();
			//  Retrieve back from memory as it might have changed during malloc.
			nodeData = tree->dataObject<TR_NodeDataObject>(nodeRef);
			nodeData->drawingDataOffset = DataOffset_t(drawingAnimationData, nodeData);
			block	= nodeRef + nodeData->drawingDataOffset;
			currentSpriteMapping.clear();
			return true;
		}
	}
	return false;
}

void TR_NodeTreeBuilder::endDrawingSequence() {
	if (!tree->dataObject<TR_DrawingAnimationDataObject>(block)) {
		ISO_TRACE("No block operation on animated pivot active at this point.\n");
		return;
	}
	//  Add sprite names to drawing data.
	for (auto i = currentSpriteMapping.begin(), iEnd = currentSpriteMapping.end(); i != iEnd; ++i) {
		TR_StringDataObject*	stringData	= tree->addStringDataObject(*i);
		TR_DrawingDataObject*	drawingData	= tree->dataObject<TR_DrawingDataObject>(i.key());
		if (!drawingData) {
			ISO_TRACE("Could not find drawing data.\n");
			continue;
		}
		drawingData->drawingNameOffset = DataOffset_t(stringData, drawingData);
	}
	currentSpriteMapping.clear();
	block = nullptr;
}

void TR_NodeTreeBuilder::addDrawing(const char *spriteSheetName, const char *spriteName, float frame, float repeat) {
	if (tree->spriteSheetName && tree->spriteSheetName != spriteSheetName) {
		ISO_TRACE("A single node tree cannot refer to multiple sprite sheets.\n");
		return;
	}
	if (!tree->spriteSheetName)
		tree->spriteSheetName = spriteSheetName;

	if (TR_DrawingAnimationDataObject* drawingAnimationData = tree->dataObject<TR_DrawingAnimationDataObject>(block)) {
		++drawingAnimationData->nDrawings;
		TR_DrawingDataObject* drawingData	= tree->addDataObject<TR_DrawingDataObject>(frame, repeat);
		currentSpriteMapping[tree->dataRef(drawingData)] = spriteName;

	} else {
		ISO_TRACE("No block operation on drawing animation active at this point.\n");
	}
}

DataRef_t TR_NodeTreeBuilder::createChannelData(CurveChannel_t channelType, DataRef_t nodeRef) {
	//  Warning.  This code does not handle duplicate channels.  A duplicate channel will be added in the channel list, but might never be parsed if searching for a unique channel.
	TR_NodeDataObject* nodeData = tree->dataObject<TR_NodeDataObject>(nodeRef);
	if (!nodeData)
		return nullptr;

	DataRef_t channelOffset	= nodeData->channelDataOffset;

	//  First channel data in node data, directly hook it up.
	if (!channelOffset) {
		//  Add new channel data.
		TR_ChannelDataObject* channelData = tree->addDataObject<TR_ChannelDataObject>(channelType);

		//  Retrieve back from memory as it might have changed during malloc.
		nodeData = tree->dataObject<TR_NodeDataObject>(nodeRef);
		nodeData->channelDataOffset = DataOffset_t(channelData, nodeData);

		return tree->dataRef(channelData);

	} else {
		//  Iterate to last channel data, and append new channel data to it.
		while (channelOffset) {
			TR_DataObject* dataObject = tree->dataObject(nodeRef + channelOffset);
			if (dataObject->id == eChannelData) {
				TR_ChannelDataObject* channelData = static_cast<TR_ChannelDataObject*>(dataObject);
				nodeRef			= tree->dataRef(channelData);
				channelOffset	= channelData->nextChannelDataOffset;
			} else {
				nodeRef			= channelOffset = nullptr;
				break;
			}
		}
		if (nodeRef) {
			//  Add new channel data.
			TR_ChannelDataObject*	channelData	= tree->addDataObject<TR_ChannelDataObject>(channelType);
			TR_DataObject*			dataObject	= tree->dataObject(nodeRef);
			if (dataObject->id == eChannelData) {
				TR_ChannelDataObject* prevChannelData = static_cast<TR_ChannelDataObject*>(dataObject);
				prevChannelData->nextChannelDataOffset = DataOffset_t(channelData, prevChannelData);
				return tree->dataRef(channelData);
			}
		}
	}
	return nullptr;
}

//-----------------------------------------------------------------------------
//	Views
//-----------------------------------------------------------------------------

class TV_NodeTreeView;

class TV_ChannelView : public refs<TV_ChannelView> {
public:
	virtual ~TV_ChannelView() {}
};

class TV_FloatDataView : public inherit_refs<TV_FloatDataView, TV_ChannelView> {
public:
	virtual bool getValue(float frame, float &value) const =0;
	float getValue(float frame) const { float value = 0; getValue(frame, value); return value; }
};

class TV_Pos3dDataView : public inherit_refs<TV_Pos3dDataView, TV_ChannelView> {
public:
	virtual bool getValue(float frame, float3 &value) const =0;
};

class TV_EffectDataView : public inherit_refs<TV_EffectDataView, TV_ChannelView> {
	const TR_EffectDataObject      *effect;
public:
	ref_ptr<TV_NodeTreeView>		matte;
	TV_EffectDataView(TR_EffectDataObject *effect, TV_NodeTreeView *parent);
	bool       operator==(const TV_EffectDataView &b) const { return effect == b.effect; }
	bool       operator<(const TV_EffectDataView &b) const { return effect < b.effect; }
	EffectId_t effectId() const { return effect ? effect->effectId : eNoop; }
};

class TV_ConstantFloatDataView : public TV_FloatDataView {
	float	value;
public:
	TV_ConstantFloatDataView(const TR_FloatDataObject *data) : value(data->value) {}
	virtual bool getValue( float frame, float &value ) const { return value; }
};

class TV_LinearCurveView : public TV_FloatDataView {
	const TR_LinearCurveDataObject *data;
public:
	TV_LinearCurveView(const TR_LinearCurveDataObject *data) : data(data) {}
	virtual bool getValue(float frame, float &value) const{
		auto	pts		= data->points();
		if (pts.size() == 1 || frame < pts.front().v.x) {
			value = pts.front().v.y;
			return true;
		}
		if (frame > pts.back().v.x) {
			value = pts.back().v.y;
			return true;
		}
		const TR_LinearPointDataObject *p = lower_boundc(data->points(), frame, [](const TR_LinearPointDataObject &p, float frame) { return p.v.x <= frame; });
		value = lerp(p[-1].v.y, p[0].v.y, (frame - p[-1].v.x) / (p[0].v.x - p[-1].v.x));
		return true;
	}
	unsigned nPoints() const	{ return data->nPoints; }
};

class TV_BezierCurveView : public TV_FloatDataView {
	const TR_BezierCurveDataObject *data;
protected:
	float getValueX(float u, const TR_BezierPointDataObject *p) const {
		float	a = lerp(p[0].v.x,		p[0].right.x,	u);
		float	b = lerp(p[0].right.x,	p[1].left.x,	u);
		float	c = lerp(p[1].left.x,	p[1].v.x,		u);
		float	d = lerp(a, b,							u);
		float	e = lerp(b, c,							u);
		float	f = lerp(d, e,							u);
		return f;
	}
	float getValueY(float u, const TR_BezierPointDataObject *p) const {
		float	a = lerp(p[0].v.y,		p[0].right.y,	u);
		float	b = lerp(p[0].right.y,	p[1].left.y,	u);
		float	c = lerp(p[1].left.y,	p[1].v.y,		u);
		float	d = lerp(a, b,							u);
		float	e = lerp(b, c,							u);
		float	f = lerp(d, e,							u);
		return f;
	}
	float findU(float x, const TR_BezierPointDataObject *p) const{
		const double EPSILON  = 5e-10;
		const int    MAX_ITER = 52;

		if (x <= p[0].v.x)
			return 0;
		if (x >= p[1].v.x)
			return 1;

		float  u, v, u1 = 0,  u2 = 0;
		for (int i = 0; i < MAX_ITER; i++) {
			u = 0.5f * (u1 + u2);
			v = getValueX(u, p);

			if (v < x)
				u1 = u;
			else
				u2 = u;

			if (abs(v - x) < EPSILON)
				break;
		}
		return u;
	}
public:
	TV_BezierCurveView(const TR_BezierCurveDataObject *data) : data(data) {}

	virtual bool getValue(float frame, float &value) const {
		auto	pts = data->points();

		if (pts.size() == 1 || frame < pts.front().v.x) {
			value = pts.front().v.y;
			return true;
		}
		if (frame > pts.back().v.x) {
			value = pts.back().v.y;
			return true;
		}
		const TR_BezierPointDataObject *p = lower_boundc(data->points(), frame, [](const TR_BezierPointDataObject &p, float frame) { return p.v.x <= frame; }) - 1;
		value = p->constSeg ? float(p->v.y) : getValueY(findU(frame, p), p);
		return true;
	}
	unsigned nPoints() const { return data->nPoints; }
};

class TV_PivotDataView : public TV_Pos3dDataView {
	TR_AnimatedPivotDataObject	*data;
public:
	TV_PivotDataView(TR_AnimatedPivotDataObject *data) : data(data) {}
	virtual bool getValue(float frame, float3 &value) const {
		auto	pts		= data->points();

		if (frame < pts.front().frame) {
			value = pts.front().v;
			return true;
		}
		if (frame > pts.back().frame) {
			value = pts.back().v;
			return true;
		}
		auto	p = lower_boundc(pts, frame, [](const TR_PivotPointDataObject &p, float frame) { return p.frame <= frame; });
		value = p[-1].v;
		return true;
	}
	unsigned nPoints() const { return data->nPoints; }
};

class TV_CatmullCurveView : public TV_Pos3dDataView {
	const TR_CatmullCurveDataObject	*data;
	ref_ptr<TV_FloatDataView>		vel;

public:
	TV_CatmullCurveView(const TR_CatmullCurveDataObject *data, TV_FloatDataView *vel = nullptr) : data(data), vel(vel) {}

	virtual bool getValue(float frame, float3 &value) const {
		auto	pts		= data->points();
		auto	scale	= data->scale;

		if (frame < pts.begin()->frame) {
			value = pts.front().v;

		} else if (frame > pts.back().frame) {
			value = pts.back().v;

		} else {
			float length = pts.back().distance;
			if (length == 0) {
				value = pts.back().v;
			} else {
				float f = vel ? vel->getValue(frame)
					: pts.back().frame > 1
					? ((frame - 1) / (pts.back().frame - 1))
					: 0;

				float	distance	= f * length;
				auto	i			= lower_boundc(data->points(), distance, [](const TR_CatmullPointDataObject &p, float distance) { return p.distance <= distance; });
				Discreet point;
				getDiscreetValue(i - 1, i, distance, &point);
				value = float3{float(point.x), float(point.y), float(point.z)};
			}
		}
		value = value * scale;
		return true;
	}
	void		setVelocity(TV_FloatDataView *_vel) { vel = _vel; }
	unsigned	nPoints() const	{ return data->nPoints; }
};

constexpr float PLANE_QUANTUM = 1.f / (1024 * 64);

struct SpriteMatrix {
	float2x2	m;
	float3		z;
	SpriteMatrix(float2x3 m, float z) : m(m), z(concat(m.z, floor(z / PLANE_QUANTUM + half) * PLANE_QUANTUM)) {}
	SpriteMatrix(float2x3 m) : m(m), z(concat(m.z, zero)) {}
	SpriteMatrix& operator=(param(float2x3) _m) { m = _m; z.xy = _m.z; return *this; }

	operator float2x3() const	{ return float2x3(m, z.xy); }
	SpriteMatrix operator*(const SpriteMatrix &b) const { return SpriteMatrix(operator float2x3() * (float2x3)b, z.z + b.z.z); }
};

struct Blending {
	map<int,map<string, ref_ptr<TV_NodeTreeView>>> container;
};

singleton<Blending> blending;

float angle_lerp(float a, float b, float t) {
	float diff = b - a;
	if (diff > pi)
		diff -= pi * 2;
	else if (diff < -pi)
		diff += pi * 2;
	return lerp(a, b, t);
}

class TV_NodeTreeView : public refs<TV_NodeTreeView> {
	enum ChannelMask {
		eNodeMask        = 0,
		eAnimatedMask    = 1<<0,
		eDeformMask      = 1<<1
	};

	struct Deformation {
		float4		params;	// offset x,y; rot; len;
		Deformation() : params(zero) {}
		Deformation(param(float2) offset, float rot, float len) : params(concat(offset, rot, len)) {}
		Deformation(param(float4) params) : params(params) {}
		float2x3	start()		const	{ return translate(position2(params.xy)) * rotate2D(params.z); }
		float2x3	end()		const	{ return start() * translate(params.w, zero); }
		friend Deformation	lerp(const Deformation &a, const Deformation &b, float t) {
			return lerp(a.params, b.params, t);
		}
	};

	struct Transformation {
		float3		pivot, offset;
		float4		params;	//scle, rot, skew
		Transformation() : pivot(zero), offset(zero), params(zero) {}
		Transformation(param(float3) pivot, param(float3) offset, param(float2) scl, float rot, float skew) : pivot(pivot), offset(offset), params(concat(scl, rot, skew)) {}
		Transformation(param(float3) pivot, param(float3) offset, param(float4) params) : pivot(pivot), offset(offset), params(params) {}

		SpriteMatrix	getMat() const	{
			float2x3	m =  translate(position2(pivot.xy + offset.xy)) * rotate2D(params.z) * float2x2(x_axis, float2(sincos(params.w)).yx) * iso::scale(params.xy) * translate(position2(-pivot.xy));
			return SpriteMatrix(m, offset.z);
		}
		friend Transformation	lerp(const Transformation &a, const Transformation &b, float t) {
			return Transformation(lerp(a.pivot, b.pivot, t), lerp(a.offset, b.offset, t), lerp(a.params, b.params, t));
		}
	};

	template<typename T> struct Cached {
		unique_ptr<pair<float, T>>	p;
		bool	check(float frame) {
			if (!p)
				p = new pair<float, T>(-1, identity);
			return exchange(p->a, frame) == frame;
		}
		operator T&() { return p->b; }
	};

	const TR_NodeDataObject	*node;
	ChannelMask				channelMask;
	EffectId_t				matteId;
	ref_ptr<TV_ChannelView>	channels[MAX_CURVE_CHANNELS];

	mutable Cached<SpriteMatrix>	cachedLocalMatrix;
	mutable Cached<SpriteMatrix>	cachedModelMatrix;
	mutable Cached<float2x3>		cachedRestStartMatrix;
	mutable Cached<float2x3>		cachedRestEndMatrix;
	mutable Cached<float2x3>		cachedDeformStartMatrix;
	mutable Cached<float2x3>		cachedDeformEndMatrix;

	void createChannels();
	bool createChannel(CurveChannel_t channelType, const TR_DataObject *data);

	ref_ptr<TV_FloatDataView> floatChannel(CurveChannel_t channel) const{
		return channels[channel] ? (TV_FloatDataView*)channels[channel].get() : 0;
	}
	ref_ptr<TV_Pos3dDataView> pos3dChannel(CurveChannel_t channel) const  {
		return channels[channel] ? (TV_Pos3dDataView*)channels[channel].get() : 0;
	}
	float getFloat(float frame, CurveChannel_t channel, float value = 0) const {
		if (auto r = floatChannel(channel))
			r->getValue(frame, value);
		return value;
	}
	float2 getFloat2(float frame, CurveChannel_t channelX, CurveChannel_t channelY) const {
		return {getFloat(frame, channelX), getFloat(frame, channelY)};
	}
	float3 getFloat3(float frame, CurveChannel_t channelX, CurveChannel_t channelY, CurveChannel_t channelZ) const {
		return {getFloat(frame, channelX), getFloat(frame, channelY), getFloat(frame, channelZ)};
	}
	float3 getFloat3(float frame, CurveChannel_t channel) const {
		float3	v(zero);
		if (auto r = pos3dChannel(channel))
			r->getValue(frame, v);
		return v;
	}

public:
	ref_ptr<TV_NodeTreeView>					parent;
	dynamic_array<ref_ptr<TV_EffectDataView>>	effects;

	class BrotherIterator {
		ref_ptr<TV_NodeTreeView> node;
	public:
		BrotherIterator()	{}
		BrotherIterator(TV_NodeTreeView *node) : node(node) {}

		BrotherIterator& operator++() {
			if (node && node->isValid()) {
				TV_NodeTreeView			*parent		= node->parent;
				const TR_NodeDataObject	*nodeData	= node->node;
				if (auto next = nodeData->brother()) {
					node = new TV_NodeTreeView(next, parent, next->effect(), parent->matteId);
					return *this;
				}
			}
			node = nullptr;
			return *this;
		}
		bool operator==(const BrotherIterator& it) const {
			return !node || !node->node
				? (!it.node || !it.node->node)
				: (it.node && node->node == it.node->node);
		}

		BrotherIterator	 operator++(int)			{ auto	t = *this; operator++(); return t; }
		bool operator!=(const BrotherIterator& it) const { return !operator==(it); }

		const TV_NodeTreeView& operator*() const	{ return *node; }
		TV_NodeTreeView&	   operator*()			{ return *node; }
		const TV_NodeTreeView* operator->() const	{ return node; }
		TV_NodeTreeView*	   operator->()			{ return node; }
	};

	TV_NodeTreeView(const TR_NodeDataObject *node, TV_NodeTreeView *parent, TR_EffectDataObject *effect = 0, EffectId_t matteId = eNoop)
		: node(node), matteId(matteId), parent(parent)
	{
		createChannels();
		if (effect)
			effects.push_back(new TV_EffectDataView(effect, parent));
		if (parent)
			effects.insert(effects.end(), parent->effects.begin(), parent->effects.end());
	}

	bool	isValid()			const	{ return !!node; }
	auto	name()				const	{ return node->name(); }

	TV_NodeTreeView	*child() const {
		if (auto c = node->child())
			return new TV_NodeTreeView(c, unconst(this), c->effect());
		return nullptr;
	}
	auto	children() const {
		return make_range(BrotherIterator(child()), BrotherIterator());
	}

	bool	operator==(const TV_NodeTreeView& b)	const	{ return this == &b || node == b.node; }
	bool	operator<(const TV_NodeTreeView& b)		const	{ return node < b.node; }

	auto	sprites() const {
		if (node) {
			if (auto ad = node->drawing())
				return ad->entries();
		}
		return range<TR_DrawingDataObject*>(none);
	}

	ref_ptr<TV_NodeTreeView> find(const char *nodeName)	const {
		if (name() == nodeName)
			return unconst(this);
		for (auto &i : children()) {
			if (auto r = i.find(nodeName))
				return r;
		}
		return nullptr;
	}

	template<typename P> ref_ptr<TV_NodeTreeView> findIf(const P& pred) const {
		if (pred(*this))
			return unconst(this);
		for (auto &i : children()) {
			if (auto r = i.findIf(pred))
				return r;
		}
		return nullptr;
	}

	float totalDuration() const {
		float currentDuration = 0;
		for (auto &i : sprites())
			currentDuration = max(currentDuration, i.frame + i.repeat - 1);
		for (auto &i : children())
			currentDuration = max(currentDuration, i.totalDuration());
		return currentDuration;
	}

	unsigned depth() const {
		unsigned depth = 0;
		for (auto i = parent; i; i = i->parent)
			++depth;
		return depth;
	}

	float3 position(float frame) const {
		if (auto xyz = pos3dChannel(eXYZ)) {
			float3	pos;
			if (xyz->getValue(frame, pos))
				return pos;
		}
		return getFloat3(frame, eSeparateX, eSeparateY, eSeparateZ);
	}

	float2 scale(float frame) const {
		if (auto xy = floatChannel(eScaleXY)) {
			float	scale;
			if (xy->getValue(frame, scale))
				return scale;
		}
		return {getFloat(frame, eScaleX, 1), getFloat(frame, eScaleY, 1)};
	}

	Transformation transform(float frame)	const {
		return Transformation(getFloat3(frame, ePivot), position(frame), scale(frame), getFloat(frame, eRotationZ), getFloat(frame, eSkew));
	}
	Deformation	rest(float frame) const {
		return Deformation(getFloat2(frame, eRestOffsetX, eRestOffsetY), getFloat(frame, eRestRotation), getFloat(frame, eRestLength));
	}
	Deformation	deform(float frame) const {
		return Deformation(getFloat2(frame, eDeformOffsetX, eDeformOffsetY), getFloat(frame, eDeformRotation), getFloat(frame, eDeformLength));
	}

	float	opacity(float frame) const {
		return getFloat(frame, eOpacity, 1);
	}

	const SpriteMatrix&	localMatrix(float frame) const;
	const SpriteMatrix&	localMatrixWithBlending(float frameFrom, float frameTo, float fullBlendTime, float currentBlendTime, int blendID) const;
	const SpriteMatrix&	modelMatrix(float frame) const;
	const SpriteMatrix&	modelMatrixWithBlending(float frameFrom, float frameTo, float fullBlendTime, float currentBlendTime, int blendID) const;

	const float2x3&		restStartMatrix(float frame) const;
	const float2x3&		restEndMatrix(float frame) const;
	const float2x3&		deformStartMatrix(float frame) const;
	const float2x3&		deformEndMatrix(float frame) const;

	const float2x3&		restStartMatrixWithBlending(float frameFrom, float frameTo, float fullBlendTime, float currentBlendTime, int blendID) const;
	const float2x3&		restEndMatrixWithBlending(float frameFrom, float frameTo, float fullBlendTime, float currentBlendTime, int blendID) const;
	const float2x3&		deformStartMatrixWithBlending(float frameFrom, float frameTo, float fullBlendTime, float currentBlendTime, int blendID) const;
	const float2x3&		deformEndMatrixWithBlending(float frameFrom, float frameTo, float fullBlendTime, float currentBlendTime, int blendID) const;
};

TV_EffectDataView::TV_EffectDataView(TR_EffectDataObject *effect, TV_NodeTreeView *parent) : effect(effect) {
	if (auto m = effect->matte())
		matte = new TV_NodeTreeView(m, parent, 0, effect->effectId);
}

const SpriteMatrix &TV_NodeTreeView::localMatrix(float frame) const {
	if (cachedLocalMatrix.check(frame))
		return cachedLocalMatrix;

	SpriteMatrix &matrix = cachedLocalMatrix;
	matrix = identity;

	//  Node contains animated node channel data
	if (channelMask & eAnimatedMask)
		matrix = transform(frame).getMat();

	//  Node contains deform node channel data
	if (channelMask & eDeformMask ) {
		matrix = float2x3(float3x3((float2x3)matrix * deform(frame).end() * inverse(rest(frame).end())));

		for (TV_NodeTreeView *i = parent; i && (i->channelMask & eDeformMask); i = i->parent) {
			float2x3	m = i->rest(frame).end();
			matrix	= m * (float2x3)matrix * inverse(m);
		}
	}
	return matrix;
}

const SpriteMatrix& TV_NodeTreeView::modelMatrix(float frame) const {
	if (cachedModelMatrix.check(frame))
		return cachedModelMatrix;

	SpriteMatrix& matrix	= cachedModelMatrix;
	matrix = localMatrix(frame);
	for (TV_NodeTreeView *i = parent; i; i = i->parent)
		matrix = i->localMatrix(frame) * matrix;
	return matrix;
}

const float2x3& TV_NodeTreeView::restStartMatrix(float frame) const {
	if (cachedRestStartMatrix.check(frame))
		return cachedRestStartMatrix;

	float2x3& matrix	= cachedRestStartMatrix;
	if (matteId == eDeformation) {
		matrix = rest(frame).start();
		if (parent)
			matrix = parent->restEndMatrix(frame) * matrix;
	}
	return matrix;
}

const float2x3& TV_NodeTreeView::restEndMatrix(float frame) const {
	if (cachedRestEndMatrix.check(frame))
		return cachedRestEndMatrix;

	float2x3& matrix	= cachedRestEndMatrix;
	matrix = restStartMatrix(frame);
	if (matteId == eDeformation)
		matrix = matrix * translate(getFloat(frame, eRestLength), zero);
	return matrix;
}

const float2x3& TV_NodeTreeView::deformStartMatrix(float frame) const {
	if (cachedDeformStartMatrix.check(frame))
		return cachedDeformStartMatrix;

	float2x3& matrix	= cachedDeformStartMatrix;
	if (channelMask & eDeformMask) {
		matrix = deform(frame).start();
		if (parent)
			matrix = parent->deformEndMatrix(frame) * matrix;
	} else {
		matrix = modelMatrix(frame);
	}
	return matrix;
}

const float2x3& TV_NodeTreeView::deformEndMatrix(float frame) const {
	if (cachedDeformEndMatrix.check(frame))
		return cachedDeformEndMatrix;

	float2x3& matrix	= cachedDeformEndMatrix;
	matrix = deformStartMatrix(frame);
	if (channelMask & eDeformMask)
		matrix = matrix * translate(getFloat(frame, eDeformLength), zero);
	return matrix;
}

/// for Blending///


const SpriteMatrix& TV_NodeTreeView::localMatrixWithBlending(float frameFrom, float frameTo, float fullBlendTime, float currentBlendTime, int blendID) const {
	if (cachedLocalMatrix.check(frameFrom))
		return cachedLocalMatrix;

	SpriteMatrix&		matrix	= cachedLocalMatrix;
	matrix	= identity;

	ref_ptr<TV_NodeTreeView> blend2 = blending->container[blendID][name()];
	float	t = min(currentBlendTime / fullBlendTime, 1);

	//  Node contains animated node channel data.
	if (channelMask & eAnimatedMask)
		matrix	= lerp(transform(frameFrom), blend2 ? blend2->transform(frameTo) : Transformation(), t).getMat();

	//  Node contains deform node channel data.
	if (channelMask & eDeformMask) {
		Deformation	rest2;
		Deformation	deform2;
		if (blend2) {
			rest2	= blend2->rest(frameTo);
			deform2	= blend2->deform(frameTo);
		}

		matrix = (float2x3)matrix * lerp(deform(frameFrom), deform2, t).end() * inverse(lerp(rest(frameFrom), rest2, t).end());

		for (TV_NodeTreeView *i = parent; i && (i->channelMask & eDeformMask); i = i->parent) {
			if (TV_NodeTreeView *i2 = blending->container[blendID][i->name()])
				rest2 = i2->rest(frameTo);
			else
				rest2 = Deformation();

			float2x3	m	= lerp(rest(frameFrom), rest2, t).end();
			matrix	= m * (float2x3)matrix * inverse(m);
		}
	}
	return matrix;
}

const SpriteMatrix& TV_NodeTreeView::modelMatrixWithBlending(float frameFrom, float frameTo, float fullBlendTime, float currentBlendTime, int blendID) const {
	if (cachedModelMatrix.check(frameFrom))
		return cachedModelMatrix;

	SpriteMatrix& matrix	= cachedModelMatrix;
	matrix = localMatrixWithBlending(frameFrom, frameTo, fullBlendTime, currentBlendTime, blendID);
	for (TV_NodeTreeView *i = parent; i; i = i->parent)
		matrix = i->localMatrixWithBlending(frameFrom, frameTo, fullBlendTime, currentBlendTime, blendID) * matrix;
	return matrix;
}

const float2x3& TV_NodeTreeView::restStartMatrixWithBlending(float frameFrom, float frameTo, float fullBlendTime, float currentBlendTime, int blendID) const {
	if (cachedRestStartMatrix.check(frameFrom))
		cachedRestStartMatrix;

	float2x3&	matrix	= cachedRestStartMatrix;

	if (matteId == eDeformation) {
		ref_ptr<TV_NodeTreeView>	blend2	= blending->container[blendID][name()];
		float	t = min(currentBlendTime / fullBlendTime, 1);
		matrix = lerp(rest(frameFrom), blend2 ? blend2->rest(frameFrom) : Deformation(), t).start();
		if (parent)
			matrix = parent->restEndMatrixWithBlending(frameFrom, frameTo, fullBlendTime, currentBlendTime, blendID) * matrix;

	} else {
		matrix = modelMatrixWithBlending(frameFrom, frameTo, fullBlendTime, currentBlendTime, blendID);
	}
	return matrix;
}

const float2x3& TV_NodeTreeView::restEndMatrixWithBlending(float frameFrom, float frameTo, float fullBlendTime, float currentBlendTime, int blendID) const {
	if (cachedRestEndMatrix.check(frameFrom))
		return cachedRestEndMatrix;

	float2x3&	matrix	= cachedRestEndMatrix;
	matrix = restStartMatrixWithBlending(frameFrom,frameTo,fullBlendTime,currentBlendTime,blendID);

	if (matteId == eDeformation) {
		ref_ptr<TV_NodeTreeView>	blend2 = blending->container[blendID][name()];
		float	t = min(currentBlendTime / fullBlendTime, 1);
		matrix = matrix * translate(lerp(getFloat(frameFrom, eRestLength), blend2->getFloat(frameTo, eRestLength), t), zero);
	}
	return matrix;
}

const float2x3& TV_NodeTreeView::deformStartMatrixWithBlending(float frameFrom, float frameTo, float fullBlendTime, float currentBlendTime, int blendID) const {
	if (cachedDeformStartMatrix.check(frameFrom))
		return cachedDeformStartMatrix;

	float2x3&	matrix	= cachedDeformStartMatrix;

	if (channelMask & eDeformMask) {
		ref_ptr<TV_NodeTreeView>	blend2 = blending->container[blendID][name()];
		float	t = min(currentBlendTime / fullBlendTime, 1);
		matrix = lerp(deform(frameFrom), blend2 ? blend2->deform(frameTo) : Deformation(), t).start();
		if (parent)
			matrix = parent->deformEndMatrixWithBlending(frameFrom, frameTo, fullBlendTime, currentBlendTime, blendID) * matrix;

	} else {
		matrix = modelMatrixWithBlending(frameFrom, frameTo, fullBlendTime, currentBlendTime, blendID);
	}
	return matrix;
}

const float2x3& TV_NodeTreeView::deformEndMatrixWithBlending(float frameFrom, float frameTo, float fullBlendTime, float currentBlendTime, int blendID) const {
	if (cachedDeformEndMatrix.check(frameFrom))
		return cachedDeformEndMatrix;

	float2x3& matrix	= cachedDeformEndMatrix;
	matrix = deformStartMatrixWithBlending(frameFrom, frameTo, fullBlendTime, currentBlendTime, blendID);

	if (channelMask & eDeformMask) {
		ref_ptr<TV_NodeTreeView>	blend2 = blending->container[blendID][name()];
		float	t = min(currentBlendTime / fullBlendTime, 1);
		matrix = matrix * translate(lerp(getFloat(frameFrom, eDeformLength), blend2->getFloat(frameTo, eDeformLength), t), zero);
	}
	return matrix;
}

///------------///

bool TV_NodeTreeView::createChannel(CurveChannel_t channelType, const TR_DataObject *data) {
	if (data) {
		//  Float channel
		if (TV_FloatDataView* floatDataView
			= data->id == eBezierCurveData	? (TV_FloatDataView*)new TV_BezierCurveView((TR_BezierCurveDataObject*)data)
			: data->id == eLinearCurveData	? (TV_FloatDataView*)new TV_LinearCurveView((TR_LinearCurveDataObject*)data)
			: data->id == eFloatData		? (TV_FloatDataView*)new TV_ConstantFloatDataView((TR_FloatDataObject*)data)
			: 0
		) {
			channels[channelType] = floatDataView;
			if (between(channelType, eSeparateX, ePivot))
				channelMask = ChannelMask(channelMask | eAnimatedMask);
			else if (between(channelType, eRestOffsetX, eDeformRotation))
				channelMask = ChannelMask(channelMask | eDeformMask);
			return true;
		}

		//  Pos3d channel
		if (TV_Pos3dDataView* pos3dDataView
			= data->id == eCatmullCurveData		? (TV_Pos3dDataView*)new TV_CatmullCurveView((TR_CatmullCurveDataObject*)data, nullptr /* empty velocity .. will be filled in later */)
			: data->id == eAnimatedPivotData	? (TV_Pos3dDataView*)new TV_PivotDataView((TR_AnimatedPivotDataObject*)data)
			: 0
		) {
			channels[channelType] = pos3dDataView;
			channelMask = ChannelMask(channelMask | eAnimatedMask);
			return true;
		}
	}
	return false;
}

void TV_NodeTreeView::createChannels() {
	if (isValid()) {
		for (TR_ChannelDataObject* channelData = node->channel(); channelData; channelData = channelData->nextChannel())
			createChannel(channelData->channelType, channelData->linked());

		//  Once all channels have been created, set velocity in catmull data view if applicable.
		if (auto xyz = pos3dChannel(eXYZ)) {
			if (auto vel = floatChannel(eVelocity)) {
				TV_CatmullCurveView* catmullData = static_cast<TV_CatmullCurveView*>(xyz.get());
				catmullData->setVelocity(vel);
			}
		}
	}
}

//-----------------------------------------------------------------------------
//	RD_SpriteSheetCore
//-----------------------------------------------------------------------------

struct SpriteData {
	int			x, y, w, h;
	float2x3	matrix;
	SpriteData() {}
	SpriteData(int x, int y, int w, int h, float offsetX, float offsetY, float scaleX, float scaleY, float angleDegrees)
		: x(x), y(y), w(w), h(h)
		, matrix(rotate2D(degrees(-angleDegrees)) * scale(scaleX, scaleY) * translate(offsetX, offsetY))
	{}
};

class RD_SpriteSheetCore {
public:
	map<string, SpriteData>	sprites;
	ISO_ptr<bitmap>			bm;

	void addSprite(const char *name, const SpriteData &data) {
		sprites[name] = data;
	}
public:
	const SpriteData* sprite(const char *name) const {
		auto	i = sprites.find(name);
		return i != sprites.end() ? &*i : NULL;
	}
};

//template void iso::deleter<TR_NodeTree>(void*);

class RD_ClipDataCore {
public:
	struct SoundData {
		string			name;
		float			startFrame;
	};

	dynamic_array<pair<string, unique_ptr<TR_NodeTree>>>	nodeTrees;
	dynamic_array<SoundData>							soundEvents;

	const char	*name(unsigned idx)							const	{ return idx < nodeTrees.size() ? nodeTrees[idx].a : 0; }
	size_t		count()										const	{ return nodeTrees.size(); }
	void		addNodeTree(TR_NodeTree *tree, const char *name)	{ nodeTrees.emplace_back(name, tree); }
	void		addSoundEvent(const char *name, float startFrame)	{ soundEvents.push_back(SoundData{name, startFrame}); }
};

//-----------------------------------------------------------------------------
//	reader
//-----------------------------------------------------------------------------

struct SimpleNodeTree : refs<SimpleNodeTree> {
	enum Type { eNone, eRead, ePeg, eCutter, eInverseCutter, eMatte, eDeformation, eDeformationRoot };
	string	name, id;
	Type	type;
	dynamic_array<ref_ptr<SimpleNodeTree>> children;

	SimpleNodeTree(const char *name, const char *id, Type type = ePeg)	: name(name), id(id), type(type) {}
	SimpleNodeTree(const char *name, Type type = ePeg)					: name(name), id(name), type(type) {}

	ref_ptr<SimpleNodeTree> find(const char *_id) const {
		if (id == _id)
			return const_cast<SimpleNodeTree*>(this);
		for (auto &i : children) {
			if (ref_ptr<SimpleNodeTree> p = i->find(_id))
				return p;
		}
		return nullptr;
	}

	ref_ptr<SimpleNodeTree> append(const char *name, const char *id, Type type = ePeg)	{ return append(new SimpleNodeTree(name, id, type)); }
	ref_ptr<SimpleNodeTree> append(const char *name, Type type = ePeg)					{ return append(new SimpleNodeTree(name, type)); }
	ref_ptr<SimpleNodeTree> append(SimpleNodeTree *p)									{ return children.push_back(p); }
	ref_ptr<SimpleNodeTree> remove(const char *id) {
		for (auto &i : children) {
			if (i->id == id) {
				auto	old = i;
				children.erase(&i);
				return old;
			} else if (auto p = i->remove(id)) {
				return p;
			}
		}
		return nullptr;
	}

	void addToBuilder(TR_NodeTree* tree, DataRef_t parentNodeRef = DataRef_t(0)) const;
};

void SimpleNodeTree::addToBuilder(TR_NodeTree* tree, DataRef_t parentNodeRef) const {
	TR_NodeTreeBuilder builder(tree);

	//  Add in reverse order that is natural order of composition.
	for (auto &i : children) {
		switch (i->type) {
			case eRead:
				i->addToBuilder(tree, builder.addNode(i->name, parentNodeRef));
				break;

			case ePeg: {
				DataRef_t nodeRef = builder.addNode(i->name, parentNodeRef);
				//  Create an empty drawing sequence so that no drawing sequence is assigned to this node.
				builder.beginDrawingSequence(nodeRef);
				builder.endDrawingSequence();
				i->addToBuilder(tree, nodeRef);
				break;
			}
			case eCutter: {
				DataRef_t nodeRef = builder.addNode(i->name, parentNodeRef);
				if (builder.createEffect(nodeRef, ::eCutter))
					i->addToBuilder(tree, nodeRef);
				break;
			}
			case eInverseCutter: {
				DataRef_t nodeRef = builder.addNode(i->name, parentNodeRef);
				if (builder.createEffect(nodeRef, ::eInverseCutter))
					i->addToBuilder(tree, nodeRef);
				break;
			}
			case eMatte:
				i->addToBuilder(tree, builder.addMatteNode(i->name, parentNodeRef));
				break;

			case eDeformation: {
				DataRef_t nodeRef = builder.addNode(i->name, parentNodeRef);
				if (builder.createEffect(nodeRef, ::eDeformation))
					i->addToBuilder(tree, nodeRef);
				break;
			}
			case eDeformationRoot:
				i->addToBuilder(tree, builder.addMatteNode(i->name, parentNodeRef));
				break;
		}
	}
}


struct ToonBoomXML {
	filename	folder;

	ToonBoomXML(const char *folder) : folder(folder) {}

	void loadStageClip(const char *clipName, RD_ClipDataCore* clip);
	void loadSkeleton(const char *skeletonName, TR_NodeTree* tree);
	void loadAnimation(const char *animationName, TR_NodeTree* tree);
	void loadDrawingAnimation(const char *drawingAnimationName, TR_NodeTree* tree);
	void loadSpriteSheet(const char *sheetName, const char *sheetResolution, RD_SpriteSheetCore* sheet);

	dynamic_array<pair<string,string>>	loadSpriteSheetNames();
	dynamic_array<string>				loadClipNames();
};

void ToonBoomXML::loadStageClip(const char *clipName, RD_ClipDataCore* clip) {
	FileInput				file(filename(folder).add_dir("stage.xml"));
	XMLreader				xml(file);
	XMLreader::Data	data;

	for (XMLiterator it(xml, data); it.Next();) {
		if (data.Is("stages")) {
			for (it.Enter(); it.Next();) {
				if ((data.Is("stage") || data.Is("props")) && str(clipName) == data["name"]) {
					for (it.Enter(); it.Next();) {
						if (data.Is("play") || data.Is("prop")) {
							TR_NodeTree *tree = new TR_NodeTree;

							if (auto s = data.Find("skeleton"))
								loadSkeleton(s, tree);

							if (auto s = data.Find("animation"))
								loadAnimation(s, tree);

							if (auto s = data.Find("drawingAnimation"))
								loadDrawingAnimation(s, tree);

							clip->addNodeTree(tree->finalize(), data.Find("name"));

						} else if (data.Is("sound")) {
							clip->addSoundEvent(data.Find("name"), data.Get("time", 1.0f));
						}
					}
				}
			}
		}
	}
}

void ToonBoomXML::loadSkeleton(const char *skeletonName, TR_NodeTree* tree) {
	FileInput				file(filename(folder).add_dir("skeleton.xml"));
	XMLreader				xml(file);
	XMLreader::Data	data;

	for (XMLiterator it(xml, data); it.Next();) {
		if (data.Is("skeletons")) {
			for (it.Enter(); it.Next();) {
				if (data.Is("skeleton") && str(skeletonName) == data["name"]) {
					ref_ptr<SimpleNodeTree>	buildingTree = new SimpleNodeTree(0);

					for (it.Enter(); it.Next();) {
						if (data.Is("nodes")) {
							for (it.Enter(); it.Next();) {
								SimpleNodeTree::Type type =
									data.Is("peg")				? SimpleNodeTree::ePeg
								:	data.Is("read")				? SimpleNodeTree::eRead
								:	data.Is("cutter")			? SimpleNodeTree::eCutter
								:	data.Is("inverseCutter")	? SimpleNodeTree::eInverseCutter
								:	data.Is("matte")			? SimpleNodeTree::eMatte
								:	data.Is("deform")			? SimpleNodeTree::eDeformation
								:	data.Is("deformRoot")		? SimpleNodeTree::eDeformationRoot
								:	data.Is("bone")				? SimpleNodeTree::ePeg
								:	SimpleNodeTree::eNone;

								if (type != SimpleNodeTree::eNone) {
									if (auto name = data.Find("name")) {
										auto id	= data.Find("id");
										if (!buildingTree->find(id))
											buildingTree->append(name, id, type);
									}
								}
							}
						} else if (data.Is("links")) {
							for (it.Enter(); it.Next();) {
								if (data.Is("link")) {
									const char *in, *out;
									if ((in = data.Find("in")) && (out = data.Find("out"))) {
										ref_ptr<SimpleNodeTree> in_tree = buildingTree->find(in);
										if (!in_tree)
											in_tree = buildingTree->append(in);

										ref_ptr<SimpleNodeTree> out_tree = buildingTree->remove(out);
										if (!out_tree)
											out_tree = new SimpleNodeTree(out);

										in_tree->append(out_tree);
									}
								}
							}
						}
					}
					buildingTree->addToBuilder(tree);
				}
			}
		}
	}
}

struct ChannelBuilder : TR_NodeTreeBuilder {
	typedef pair<DataRef_t, CurveChannel_t>	AttrLinkData;
	typedef set<AttrLinkData>				AttrLinkCol_t;

	set<DataRef_t>		duplicateChannels;
	DataRef_t			channelDataRef;

	template<typename T> ChannelBuilder(TR_NodeTree *tree, const AttrLinkCol_t &attrLinks, const T &t) : TR_NodeTreeBuilder(tree) {
		bool	first = true;
		for (auto &i : attrLinks) {
			if (first) {
				channelDataRef	= createChannelData(i.b, i.a);
				block			= tree->dataRef(tree->addDataObject<T>(t));
				tree->dataObject<TR_ChannelDataObject>(channelDataRef)->linkedDataOffset = block - channelDataRef;
				first			= false;
			} else {
				duplicateChannels.insert(createEmptyChannel(i.b, i.a));
			}
		}
	}
	~ChannelBuilder() {
		for (auto &i : duplicateChannels)
			linkChannel(channelDataRef, i);
	}

	template<typename T> void addPoint(const T &pt) {
		if (auto *holder = tree->dataObject<typename T::holder_t>(block)) {
			++holder->nPoints;
			tree->addDataObject<T>(pt);
		} else {
			ISO_TRACE("No block operation at this point.\n");
		}
	}

};

void ToonBoomXML::loadAnimation(const char *animationName, TR_NodeTree* tree) {
	typedef pair<DataRef_t, CurveChannel_t>	AttrLinkData;
	typedef set<AttrLinkData>				AttrLinkCol_t;

	map<string, AttrLinkCol_t>	tvLinks;

	FileInput				file(filename(folder).add_dir("animation.xml"));
	XMLreader				xml(file);
	XMLreader::Data	data;

	for (XMLiterator it(xml, data); it.Next();) {
		if (data.Is("animations")) {
			for (it.Enter(); it.Next();) {
				if (data.Is("animation") && str(animationName) == data["name"]) {

					set<string>	linkedNodes;

					for (it.Enter(); it.Next();) {
						if (data.Is("attrlinks")) {
							for (it.Enter(); it.Next();) {
								if (data.Is("attrlink")) {
									if (const char *nodeName = data.Find("node")) {
										//  Retrieve first node with specified name.  Create animation data and link to that node.
										DataRef_t		nodeRef = tree->nodeDataRef(nodeName);
										CurveChannel_t	channel = data["attr"];
										if (nodeRef) {
											if (const char *tvName = data.Find("timedvalue"))
												tvLinks[tvName].insert(AttrLinkData(nodeRef, channel));

											else if (auto value = data["value"])
												TR_NodeTreeBuilder(tree).addConstantValue(channel, nodeRef, value);

											// Add node to linkedNodes collection
											// Once all functions have been created, we will link animation data to the duplicate nodes so that they share the same data
											linkedNodes.insert(nodeName);
										}
									}
								}
							}

						} else if (data.Is("timedvalues")) {
							for (it.Enter(); it.Next();) {
								if (data.Is("bezier")) {
									if (const char *name = data.Find("name")) {
										auto i = tvLinks.find(name);
										if (i != tvLinks.end()) {
											ChannelBuilder	builder(tree, *i, TR_BezierCurveDataObject());

											for (it.Enter(); it.Next();) {
												if (data.Is("pt")) {
													TR_BezierPointDataObject	pt;
													for (auto &i : data.Attributes()) {
														if (i.name == "x")
															pt.v.x = i.value;
														else if (i.name == "y")
															pt.v.y = i.value;
														else if (i.name == "rx")
															pt.right.x = i.value;
														else if (i.name == "ry")
															pt.right.y = i.value;
														else if (i.name == "lx")
															pt.left.x = i.value;
														else if (i.name == "ly")
															pt.left.y = i.value;
														else if (i.name == "constSeg")
															pt.constSeg = i.value;
													}
													builder.addPoint(pt);
												}
											}
										}
									}

								} else if (data.Is("catmull")) {
									if (const char *name = data.Find("name")) {
										auto i = tvLinks.find(name);
										if (i != tvLinks.end()) {
											TR_CatmullCurveDataObject	catmull;
											for (auto &i : data.Attributes()) {
												if (i.name == "scaleX")
													catmull.scale.x = i.value;
												else if (i.name == "scaleY")
													catmull.scale.y = i.value;
												else if (i.name == "scaleZ")
													catmull.scale.z = i.value;
											}

											ChannelBuilder		builder(tree, *i, catmull);

											for (it.Enter(); it.Next();) {
												if (data.Is("pt")) {
													TR_CatmullPointDataObject	pt;
													for (auto &i : data.Attributes()) {
														if (i.name == "lockedInTime")
															pt.frame = i.value;
														else if (i.name == "x")
															pt.v.x = i.value;
														else if (i.name == "y")
															pt.v.y = i.value;
														else if (i.name == "z")
															pt.v.z = i.value;
														else if (i.name == "tension")
															pt.tension = i.value;
														else if (i.name == "continuity")
															pt.continuity = i.value;
														else if (i.name == "bias")
															pt.bias = i.value;
													}
													builder.addPoint(pt);
												}
											}
										}
									}

								} else if (data.Is("linear")) {
									if (const char *name = data.Find("name")) {
										auto i = tvLinks.find(name);
										if (i != tvLinks.end()) {
											ChannelBuilder	builder(tree, *i, TR_LinearCurveDataObject());

											for (it.Enter(); it.Next();) {
												if (data.Is("pt")) {
													TR_LinearPointDataObject	pt;
													for (auto &i : data.Attributes()) {
														if (i.name == "x")
															pt.v.x = i.value;
														else if (i.name == "y")
															pt.v.y = i.value;
													}
													builder.addPoint(pt);
												}
											}
										}
									}

								} else if (data.Is("pivot")) {
									if (const char *name = data.Find("name")) {
										auto i = tvLinks.find(name);
										if (i != tvLinks.end()) {
											ChannelBuilder	builder(tree, *i, TR_AnimatedPivotDataObject());

											for (it.Enter(); it.Next();) {
												if (data.Is("pt")) {
													TR_PivotPointDataObject	pt;
													for (auto &i : data.Attributes()) {
														if (i.name == "start")
															pt.frame = i.value;
														else if (i.name == "x")
															pt.v.x = i.value;
														else if (i.name == "y")
															pt.v.y = i.value;
														else if (i.name == "z")
															pt.v.z = i.value;
													}
													builder.addPoint(pt);
												}
											}
										}
									}
								}
							}
						}
					}

					for (auto &i : linkedNodes) {
						TR_NodeTree::DataRefCol_t dataRefs;
						tree->nodeDataRefs(i, dataRefs);
						auto		iDataRef	= dataRefs.begin();
						auto		iDataRefEnd	= dataRefs.end();
						DataRef_t	srcDataRef	= *iDataRef;

						for (++iDataRef; iDataRef != iDataRefEnd ; ++iDataRef)
							TR_NodeTreeBuilder(tree).linkNodeAnimation(srcDataRef, *iDataRef);
					}

				}
			}
		}
	}
}

void ToonBoomXML::loadDrawingAnimation(const char *drawingAnimationName, TR_NodeTree* tree) {
	FileInput				file(filename(folder).add_dir("drawingAnimation.xml"));
	XMLreader				xml(file);
	XMLreader::Data	data;

	for (XMLiterator it(xml, data); it.Next();) {
		if (data.Is("drawingAnimations")) {
			for (it.Enter(); it.Next();) {
				string	spriteSheetName;
				if (data.Is("drawingAnimation") && str(drawingAnimationName) == data["name"] && (spriteSheetName = data.Find("spritesheet"))) {

					for (it.Enter(); it.Next();) {
						if (data.Is("drawing")) {
							if (const char *nodeName = data.Find("node")) {
								TR_NodeTreeBuilder	builder(tree);
								set<DataRef_t>	dataRefs;
								tree->nodeDataRefs(nodeName, dataRefs);

								auto			itRef	= dataRefs.begin();
								auto			itEnd	= dataRefs.end();

								//  Start drawing animation sequence on first valid node.
								while (itRef != itEnd) {
									if (builder.beginDrawingSequence(*itRef))
										break;
									++itRef;
								}

								for (it.Enter(); it.Next();) {
									if (itRef == itEnd)
										break;

									if (data.Is("drw")) {
										if (const char *drawingName = data.Find("name")) {
											float frame	 = 1;
											float repeat = 1;
											for (auto &i : data.Attributes()) {
												if (i.name == "frame")
													frame = i.value;
												else if (i.name == "repeat")
													repeat = i.value;
											}
											builder.addDrawing(spriteSheetName, drawingName, frame, repeat);
										}
									}
								}

								//  Stop drawing animation sequence.
								builder.endDrawingSequence();

								//  Iterate on other nodes and link drawing animation if available.
								DataRef_t srcDataRef = *itRef++;
								while (itRef != itEnd)
									builder.linkNodeDrawingAnimation(srcDataRef, *itRef++);

							}
						}
					}
				}
			}
		}
	}
}

void ToonBoomXML::loadSpriteSheet(const char *sheetName, const char *sheetResolution, RD_SpriteSheetCore* sheet) {
	FileInput				file(filename(folder).add_dir("spriteSheets.xml"));
	XMLreader				xml(file);
	XMLreader::Data	data;

	for (XMLiterator it(xml, data); it.Next();) {
		if (data.Is("spritesheets")) {
			for (it.Enter(); it.Next();) {
				if (data.Is("spritesheet")
					&& str(sheetName)		== data["name"]
					&& str(sheetResolution)	== data["resolution"]
				) {
					//  Retrieve sprite sheet filename.
					sheet->bm.CreateExternal(folder.relative(data.Find("filename")));

					for (it.Enter(); it.Next();) {
						if (data.Is("sprite")) {

							const char *nameValue = 0;
							int		x, y, w, h;
							float	scaleX = 1, scaleY = 1;
							float	offsetX = 0, offsetY = 0;
							float	angleDegrees = 0;

							for (auto &i : data.Attributes()) {
								if (i.name == "name")
									nameValue = i.value;
								else if (i.name == "rect")
									string_scan(i.value) >> x >> ',' >> y >> ',' >> w >> ',' >> h;
								else if (i.name == "offsetX")
									offsetX = i.value;
								else if (i.name == "offsetY")
									offsetY = i.value;
								else if (i.name == "scaleX")
									scaleX = i.value;
								else if (i.name == "scaleY")
									scaleY = i.value;
								else if (i.name == "angleDegrees")
									angleDegrees = i.value;
							}

							sheet->addSprite(nameValue, SpriteData(x, y, w, h, offsetX, offsetY, scaleX, scaleY, angleDegrees));
						}
					}
				}
			}
		}
	}
}

dynamic_array<pair<string,string>> ToonBoomXML::loadSpriteSheetNames() {
	FileInput				file(filename(folder).add_dir("spriteSheets.xml"));
	XMLreader				xml(file);
	XMLreader::Data	data;

	dynamic_array<pair<string,string>>	names;
	for (XMLiterator it(xml, data); it.Next();) {
		if (data.Is("spritesheets")) {
			for (it.Enter(); it.Next();) {
				if (data.Is("spritesheet"))
					names.emplace_back(data["name"], data["resolution"]);
			}
		}
	}
	return names;
}

dynamic_array<string> ToonBoomXML::loadClipNames() {
	dynamic_array<string> names;
	FileInput				file(filename(folder).add_dir("stage.xml"));
	XMLreader				xml(file);
	XMLreader::Data	data;

	for (XMLiterator it(xml, data); it.Next();) {
		if (data.Is("stages")) {
			for (it.Enter(); it.Next();) {
				if (data.Is("stage") || data.Is("props"))
					names.push_back(data["name"]);
			}
		}
	}
	return names;
}

//-----------------------------------------------------------------------------
//	ISO
//-----------------------------------------------------------------------------

template<> struct ISO::def<TR_DataObject> : ISO::VirtualT2<TR_DataObject> {
	static ISO::Browser2 Deref(TR_DataObject &d) {
		switch (d.id) {
			//case eNullData:
			case eNodeData:				return MakeBrowser(static_cast<TR_NodeDataObject&            >(d));
			case eChannelData:			return MakeBrowser(static_cast<TR_ChannelDataObject&         >(d));
			case eBezierCurveData:		return MakeBrowser(static_cast<TR_BezierCurveDataObject&     >(d));
			case eBezierPointData:		return MakeBrowser(static_cast<TR_BezierPointDataObject&     >(d));
			case eCatmullCurveData:		return MakeBrowser(static_cast<TR_CatmullCurveDataObject&    >(d));
			case eCatmullPointData:		return MakeBrowser(static_cast<TR_CatmullPointDataObject&    >(d));
			case eLinearCurveData:		return MakeBrowser(static_cast<TR_LinearCurveDataObject&     >(d));
			case eLinearPointData:		return MakeBrowser(static_cast<TR_LinearPointDataObject&     >(d));
			case eAnimatedPivotData:	return MakeBrowser(static_cast<TR_AnimatedPivotDataObject&   >(d));
			case ePivotPointData:		return MakeBrowser(static_cast<TR_PivotPointDataObject&      >(d));
			case eDrawingAnimationData:	return MakeBrowser(static_cast<TR_DrawingAnimationDataObject&>(d));
			case eDrawingData:			return MakeBrowser(static_cast<TR_DrawingDataObject&         >(d));
			case eFloatData:			return MakeBrowser(static_cast<TR_FloatDataObject&           >(d));
			//case eStringData:			return MakeBrowser(static_cast<TR_StringDataObject&          >(d));
			case eEffectData:			return MakeBrowser(static_cast<TR_EffectDataObject&          >(d));
			default:					return Browser2();
		}
	}
};

ISO_DEFUSERCOMPV(SpriteData, x, y, w, h, matrix);
ISO_DEFUSERCOMPV(RD_SpriteSheetCore, bm, sprites);

#define ENUM(x)	, #x, x
ISO_DEFUSERENUM(CurveChannel_t, MAX_CURVE_CHANNELS) {
	Init(0 VA_APPLY(ENUM,
		eNullChannel,eSeparateX,eSeparateY,eSeparateZ,eXYZ,eVelocity,eScaleX,eScaleY,
		eScaleXY,eRotationZ,eSkew,ePivot,eRestOffsetX,eRestOffsetY,eRestLength,eRestRotation,
		eDeformOffsetX,eDeformOffsetY,eDeformLength,eDeformRotation,eOpacity
	));
}};

ISO_DEFUSERCOMPV(TR_NodeDataObject, channel, drawing, effect, brother, child);
ISO_DEFUSERCOMPV(TR_EffectDataObject, matte);
ISO_DEFUSERCOMPV(TR_ChannelDataObject, channelType, linked, nextChannel);
ISO_DEFUSERCOMPV(TR_BezierPointDataObject, v, left, right, constSeg);
ISO_DEFUSERCOMPV(TR_BezierCurveDataObject, points);
ISO_DEFUSERCOMPV(TR_CatmullPointDataObject, frame, v, tension, continuity, bias, distance, d_in, d_out);
ISO_DEFUSERCOMPV(TR_CatmullCurveDataObject, scale, points);
ISO_DEFUSERCOMPV(TR_LinearCurveDataObject, points);
ISO_DEFUSERCOMPV(TR_LinearPointDataObject, v);
ISO_DEFUSERCOMPV(TR_AnimatedPivotDataObject, points);
ISO_DEFUSERCOMPV(TR_PivotPointDataObject, frame, v);
ISO_DEFUSERCOMPV(TR_DrawingAnimationDataObject, entries);
ISO_DEFUSERCOMPV(TR_DrawingDataObject, frame, repeat, name);
ISO_DEFUSERCOMPV(TR_FloatDataObject, value);


ISO_DEFUSERCOMPV(TR_NodeTree, root);
ISO_DEFUSERCOMPV(RD_ClipDataCore::SoundData, name, startFrame);
ISO_DEFUSERCOMPV(RD_ClipDataCore, nodeTrees, soundEvents);

struct ToonBoom : anything {};
ISO_DEFUSER(ToonBoom, anything);

class ToonBoomXMLFileHandler : public FileHandler {
	const char*		GetDescription() override { return "ToonBoom Exported Animation"; }
	int				Check(const filename &fn) override {
		return is_dir(fn) && exists(filename(fn).add_dir("stage.xml")) ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	ReadWithFilename(tag id, const filename &fn) override {
		ToonBoomXML	tb(fn);

		ISO_ptr<ToonBoom>	p(id);

		for (auto &i : tb.loadSpriteSheetNames()) {
			ISO_ptr<RD_SpriteSheetCore>	sheet(i.a);
			tb.loadSpriteSheet(i.a, i.b, sheet);
			p->Append(sheet);
		}

		for (auto &i : tb.loadClipNames()) {
			ISO_ptr<RD_ClipDataCore>	clip(i);
			tb.loadStageClip(i, clip);
			p->Append(clip);
		}

		return p;

		RD_ClipDataCore clip;
		tb.loadStageClip("CAMERON_rig_CAMERON_rig", &clip);
		return ISO_NULL;

	}
} toonboom_xml;

#ifdef ISO_EDITOR
//-----------------------------------------------------------------------------
//	Viewer
//-----------------------------------------------------------------------------

#include "viewers/viewer.h"
#include "viewers/viewer2d.h"
#include "windows/control_helpers.h"

struct TBContext {
	d2d::Target			&target;
	RD_SpriteSheetCore	*spritesheet;
	d2d::Bitmap			bm;

	void	Draw(param(float2x3) screen, float t, const TV_NodeTreeView &tree, int depth = 0) {
		ISO_TRACEF("") << repeat("  ", depth) << tree.name() << '\n';
		for (auto &i : tree.sprites()) {
			float			opacity	= tree.opacity(t + 1);
			SpriteMatrix	smat	= tree.modelMatrix(t + 1);
			float2x3		mat		= screen * (float2x3)smat;
			if (between(t, i.frame - 1, i.frame + i.repeat)) {
				if (auto sprite = spritesheet->sprite(i.name())) {
					float2x3	flip_sprite = translate(zero, sprite->h) * scale(one, -one);
					target.SetTransform(mat * sprite->matrix * flip_sprite);

					d2d::matrix5x4	cmat(identity, float4(zero));
					cmat[3] = concat(float3(zero), opacity);

					target.DrawImage(d2d::point(0, 0),
						d2d::Effect(target, CLSID_D2D1ColorMatrix)
							.SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, cmat)
							.SetInput(bm),
						d2d::rect::with_ext(sprite->x, sprite->y, sprite->w, sprite->h));
				}
			}
		}
		for (auto &i : tree.children())
			Draw(screen, t, i, depth + 1);
	}

	TBContext(d2d::Target &target, RD_SpriteSheetCore *spritesheet) : target(target), spritesheet(spritesheet) {
		bm	= d2d::Bitmap(target, spritesheet->bm->All());
	}
};

struct TBCollector {
	struct entry {
		const char *name;
		float2x3	transform;
		float		z, opacity;
		entry(const char *name, const SpriteMatrix &mat, float opacity) : name(name), transform(mat), z(mat.z.z), opacity(opacity) {}
		bool operator<(const entry &b) const { return z < b.z; }
	};
	dynamic_array<entry>	entries;

	void	Collect(float t, const TV_NodeTreeView &tree) {
		float			opacity	= tree.opacity(t + 1);
		SpriteMatrix	mat	= tree.modelMatrix(t + 1);
		for (auto &i : tree.sprites()) {
			if (between(t, i.frame - 1, i.frame + i.repeat))
				entries.emplace_back(i.name(), mat, opacity);
		}
		for (auto &i : tree.children())
			Collect(t, i);
	}
};


class ViewToonBoom : public win::Inherit<ViewToonBoom, Viewer2D>, public win::WindowTimer<ViewToonBoom> {
	ISO_ptr_machine<ToonBoom>	p;

	RD_ClipDataCore				*clip;
	ref_ptr<TV_NodeTreeView>	root;
	float						duration;
	float						time;
	d2d::Bitmap					bm;

public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);
	ViewToonBoom(const win::WindowPos &wpos, const ISO_ptr_machine<ToonBoom> &p) : p(p), time(0) {
		clip		= p->back();
		TR_NodeTree	*tree = clip->nodeTrees[0].b;
		root		= new TV_NodeTreeView(tree->root(), 0);
		duration	= root->totalDuration();

		Create(wpos, (tag)p.ID(), CHILD | VISIBLE | CLIPCHILDREN | CLIPSIBLINGS, NOEX);

		RD_SpriteSheetCore	*ss		= (*p)[tree->spriteSheetName];
		ss->bm	= FileHandler::ExpandExternals(ss->bm);
		bm		= d2d::Bitmap(*this, ss->bm->All());
	}
};

LRESULT ViewToonBoom::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			Timer::Start(1.f / 24);
			break;

		case WM_ISO_TIMER:
			time = mod(time + 1, duration);
			Invalidate();
			break;

		case WM_KEYDOWN:
			switch (wParam) {
				case VK_SPACE:
					if (IsRunning())
						Stop();
					else
						Timer::Start(1.f / 24);
					break;
			}
			break;

		case WM_MOUSEACTIVATE:
			SetFocus();
			SetAccelerator(*this, Accelerator());
			return MA_NOACTIVATE;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case d2d::PAINT: {
					auto	*info	= (d2d::PAINT_INFO*)nmh;

					TBCollector	col;
					col.Collect(time, *root);
					sort(col.entries);

					auto	screen	= transformation() * scale(one, -one);

					RD_SpriteSheetCore	*ss		= (*p)[clip->nodeTrees[0].b->spriteSheetName];

					for (auto &i : col.entries) {
						if (auto sprite = ss->sprite(i.name)) {
							float2x3	mat			= screen * (float2x3)i.transform;
							float2x3	flip_sprite = translate(zero, sprite->h) * scale(one, -one);

							SetTransform(mat * sprite->matrix * flip_sprite);

							d2d::matrix5x4	cmat(identity, float4(zero));
							cmat[3] = concat(float3(zero), i.opacity);

							DrawImage(//d2d::point(0, 0),
								d2d::Effect(*this, CLSID_D2D1ColorMatrix)
								.SetValue(D2D1_COLORMATRIX_PROP_COLOR_MATRIX, cmat)
								.SetInput(bm),
								d2d::rect::with_ext(sprite->x, sprite->y, sprite->w, sprite->h)
							);
						}
					}
					break;
				}
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
			break;
	}
	return Super(message, wParam, lParam);
}


class EditorToonBoom : public app::Editor {
	bool Matches(const ISO::Browser &b) override {
		return b.Is("ToonBoom");
	}
	win::Control Create(app::MainWindow &main, const win::WindowPos &wpos, const ISO_ptr_machine<void> &p) override {
		return *new ViewToonBoom(wpos, p);
	}
} editor_toonboom;


#endif
