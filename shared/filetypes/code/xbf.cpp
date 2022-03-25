#include "iso/iso_files.h"
#include "base/strings.h"
//#include "clr.h"
#include "comms/leb128.h"
#include "xbf_types.h"

using namespace iso;
/*
struct PersistedXamlXmlNamespace {
	unsigned int	m_uiNamespaceUri;
};
struct PersistedXamlType {
	XamlBitSet<enum PersistedXamlType::PersistedXamlTypeFlags>	m_TypeFlags;
	unsigned int	m_uiTypeNamespacel
	unsigned int	m_uiTypeName;
};
struct PersistedXamlProperty {
	XamlBitSet<enum PersistedXamlProperty::PersistedXamlPropertyFlags>	m_PropertyFlags;
	unsigned int	m_uiType;
	unsigned int	m_uiPropertyName;
};

struct PersistedXamlSubStream {
	unsigned int	m_nodeStreamOffset;
	unsigned int	m_lineStreamOffset;
};

struct PersistedXamlAssembly {
	XamlTypeInfoProviderKind	m_eTypeInfoProviderKind;
	unsigned int	m_uiAssemblyName;
};
struct PersistedXamlTypeNamespace {
	unsigned int	m_uiAssembly;
	unsigned int	m_uiNamespaceName;
};

PersistedXamlValueNode
PersistedXamlNode2


enum XamlTypeInfoProviderKind {
	Unknown = 0,
	Native = 1,
	Managed = 2,
	System = 3,
	Parser = 4,
	Alternate = 5
};

enum PersistedXamlTypeFlags {
	TypeNone = 0,
	TypeMarkupDirective = 1
};

enum PersistedXamlPropertyFlags {
	PropNone = 0,
	PropXmlProperty = 0x01,
	PropMarkupDirective = 0x02,
	PropImplicitProperty = 0x04
};

enum XamlNodeType {
	NodeNone = 0,
	StartObject = 1,
	EndObject = 2,
	StartProperty = 3,
	EndProperty = 4,
	Text = 5,
	Value = 6,
	Namespace = 7,
	EndOfAttributes = 8,
	EndOfStream = 9,
	LineInfo = 10,
	LineInfoAbsolute = 11
};

enum Type {
	None = 0,
	BoolFalse = 1,
	BoolTrue = 2,
	Float = 3,
	Signed = 4,
	String = 5,
	KeyTime = 6,
	Thickness = 7,
	LengthConverter = 8,
	GridLength = 9,
	Color = 10,
	Duration = 11
};

PersistedXamlNode::PersistedXamlNodeFlags {
	IsRetrieved				= 1,
	IsUnknown				= 2,
	IsStringValueAndUnique	= 4,
	IsTrustedXbfIndex		= 8,
};

enum XamlDirectives {
	xdNone					= 0,
	xdKey					= 1,
	xdName					= 2,
	xdClass					= 3,
	xdUid					= 4,
	xdDeferLoadStrategy		= 5,
	xdLoad					= 6,
	xdClassModifier			= 7,
	xdFieldModifier			= 8,
	xdConnectionId			= 9,
	xdItems					= 10,
	xdInitialization		= 11,
	xdPositionalParameters	= 12,
	xdLang					= 13,
	xdBase					= 14,
	xdSpace					= 15,
};

enum XamlBinaryFormatValidator::XamlNodeStreamSymbol {
	T_UNKNOWN								= 0,
	T_END_ELEMENT_NODE						= 1,
	T_END_OF_ATTRIBUTES_NODE				= 2,
	T_END_PROPERTY_NODE						= 3,
	T_EOS									= 4,
	T_PREFIX_DEFINITION_NODE				= 5,
	T_START_ELEMENT_NODE					= 6,
	T_START_MULTI_ITEM_PROPERTY_NODE		= 7,
	T_START_SINGLE_ITEM_PROPERTY_NODE		= 8,
	T_START_XNAME_DIRECTIVE_PROPERTY_NODE	= 9,
	T_START_XUID_DIRECTIVE_PROPERTY_NODE	= 10,
	T_START_X_DIRECTIVE_PROPERTY_NODE		= 11,
	T_TEXTVALUE_NODE						= 12,
	T_VALUE_NODE							= 13,
	NT_DIRECTIVE_PROPERTY					= 14,
	NT_DIRECTIVE_PROPERTY_CONTENT			= 15,
	NT_DIRECTIVE_PROPERTIES					= 16,
	NT_DOCUMENT								= 17,
	NT_ELEMENT								= 18,
	NT_MULTI_ITEM_PROPERTY					= 19,
	NT_NAME_DIRECTIVE_PROPERTY				= 20,
	NT_PROPERTY								= 21,
	NT_PROPERTY_CONTENT						= 22,
	NT_SINGLE_ITEM_PROPERTY					= 23,
	NT_UID_DIRECTIVE_PROPERTY				= 24,
	S_PREFIX_DEFINITION_NODE_ZERO_OR_MORE	= 25,
	S_PROPERTY_CONTENT_ZERO_OR_MORE			= 26,
	S_PROPERTY_ZERO_OR_MORE					= 27,
	S_DIRECTIVE_PROPERTY_ZERO_OR_MORE		= 28,
};

struct XamlToken : XamlToken {
	uint32	m_Data;	//eg. 0x2000040a
};

struct XamlTypeToken : XamlToken {
};

struct XamlQualifiedObject {
	enum QualifiedObjectFlags
		qofNone							= 0
		qofBoxed						= 1
		qofString						= 2
		qofContainedDORequiresRelease	= 8
		qofHasPeggedManagedPeer			= 16
	};

	XamlTypeToken	m_typeToken;
	XamlBitSet<QualifiedObjectFlags>	m_flags;
	CValue			m_value;
};


*/

struct XbfColor		{ uint32 i; };
struct XbfIndex		{ uint16 i; };

struct XbfGridLength {
	uint32	type;
	float	value;
};

struct XbfBorder {
	float	left, top, right, bottom;
};

struct XbfStableIndex {
	uint16			index;
	packed<uint32>	value;
};

struct XbfString : string16 {
	bool	read(istream_ref file) {
		return string16::read(file, file.get<uint32>());
	}
};

struct XbfContext {
	template<typename T> struct Index {
		uint32	id;
		const T& operator()(const XbfContext &ctx) const { return ctx.GetEntry<T>(id); }
	};

	struct Assembly {
		uint32				providerKind;
		Index<XbfString>	name;
	};
	struct Namespace {
		Index<Assembly>		assembly;
		Index<XbfString>	name;
	};
	struct Type {
		uint32				flags;
		Index<Namespace>	namespce;
		Index<XbfString>	name;
	};
	struct Property {
		uint32				flags;
		Index<Type>			type;
		Index<XbfString>	name;
	};
	struct XmlNamespace {
		Index<XbfString>	name;
	};

	struct Section {
		uint32	nodes, lines;
	};

	dynamic_array<XbfString>		strings;
	dynamic_array<Assembly>			assemblies;
	dynamic_array<Namespace>		namespaces;
	dynamic_array<Type>				types;
	dynamic_array<Property>			properties;
	dynamic_array<XmlNamespace>		xml_namespaces;
	dynamic_array<Section>			sections;

	hash_map<string16, XbfString>	ns_prefixes;

	streamptr		data_start;

	template<typename T> const T	&GetEntry(uint32 id) const;
	template<> const XbfString		&GetEntry<XbfString>	(uint32 i) const { return strings[i]; }
	template<> const Assembly		&GetEntry<Assembly>		(uint32 i) const { return assemblies[i]; }
	template<> const Namespace		&GetEntry<Namespace>	(uint32 i) const { return namespaces[i]; }
	template<> const Type			&GetEntry<Type>			(uint32 i) const { return types[i]; }
	template<> const Property		&GetEntry<Property>		(uint32 i) const { return properties[i]; }
	template<> const XmlNamespace	&GetEntry<XmlNamespace>	(uint32 i) const { return xml_namespaces[i]; }

	XbfContext()  {}
};


struct XbfHeader : packed_types<littleendian_types> {
	char	MagicNumber[4];
	uint32	MetadataSize;
	uint32	NodeSize;
	uint32	MajorFileVersion;
	uint32	MinorFileVersion;
	uint64	StringTableOffset;
	uint64	AssemblyTableOffset;
	uint64	TypeNamespaceTableOffset;
	uint64	TypeTableOffset;
	uint64	PropertyTableOffset;
	uint64	XmlNamespaceTableOffset;
	char	Hash[64];
};

//-----------------------------------------------------------------------------
//	XBF v1
//-----------------------------------------------------------------------------

struct XamlNode {
	enum Type {
		Text = 0,
		Value = 1,
		Object = 2,
		Property = 3
	};
	Type	type;
	uint32	id;

	XamlNode(Type _type, uint32 _id = 0) : type(_type), id(_id) {};

	string	getNamespace(const XbfContext &ctx) {
		if (type != Object)
			throw string("Invalid node type for getNamespace: ") + type;

		return ctx.types[id].namespce(ctx).name(ctx);
	}

	string	getTypeName(const XbfContext &ctx) {
		if (type != Object)
			throw string("Invalid node type for getTypeName: ") + type;

		return ctx.types[id].name(ctx);
	}

	string	getPropertyName(const XbfContext &ctx) {
		if (type != Property)
			throw string("Invalid node type for getPropertyName: ") + type;

		return ctx.properties[id].name(ctx);
	}

	string	getPropertyOwner(const XbfContext &ctx) {
		if (type != Property)
			throw string("Invalid node type for getPropertyOwner: ") + type;

		return ctx.properties[id].type(ctx).name(ctx);
	}
};

struct XamlTextNode : XamlNode {
	XamlTextNode(istream_ref file) : XamlNode(Text, file.get<uint32>()) {
		uint32		flags	= file.get<uint32>();
	}
};
struct XamlValueNode : XamlNode {
	enum Type {
		None = 0,
		False = 1,
		True = 2,
		Float = 3,
		Signed = 4,
		String = 5,
		KeyTime = 6,
		Thickness = 7,
		LengthConverter = 8,
		GridLength = 9,
		Color = 10,
		Duration = 11
	};
	union {
		bool			b;
		float			f;
		int32			i;
		XbfColor		color;
		XbfString		s;
		XbfBorder		border;
		XbfGridLength	grid_length;
	};
	XamlValueNode(istream_ref file) : XamlNode(Value, file.getc()) {
		switch (id) {
			case Type::False:
				b = false;
				break;
			case Type::True:
				b = true;
				break;
			case Type::Float:
			case Type::KeyTime:
			case Type::LengthConverter:
			case Type::Duration:
				file.read(f);
				break;
			case Type::Signed:
				file.read(i);
				break;
			case Type::String:
				clear(s);
				file.read(s);
				break;
			case Type::Color:
				file.read(color);
				break;
			case Type::Thickness:
				file.read(border);
				break;
			case Type::GridLength:
				file.read(grid_length);
				break;
			case Type::None:
				// Ignore
				break;
			default:
				throw string("Unknown value node type");
		}
	}
	~XamlValueNode() {
		if (id == Type::String)
			s.~XbfString();
	}
};
struct XamlObjectNode : XamlNode {
	uint32		flags;
	dynamic_array<XamlNode*>	properties;
	XamlObjectNode(istream_ref file) : XamlNode(Object, file.get<uint32>()) {
		file.read(flags);
	}
	auto	type(const XbfContext &ctx) {
		return ctx.types[id].name(ctx);
	}
	auto	namespce(const XbfContext &ctx) {
		return ctx.types[id].namespce(ctx).name(ctx);
	}
};
struct XamlPropertyNode : XamlNode {
	uint32		flags;
	dynamic_array<XamlNode*>	values;
	XamlPropertyNode(istream_ref file) : XamlNode(Property, file.get<uint32>()) {
		file.read(flags);
	}
	auto	name(const XbfContext &ctx) {
		return ctx.properties[id].name(ctx);
	}
	auto	owner(const XbfContext &ctx) {
		return ctx.properties[id].type(ctx).name(ctx);
	}
};

struct XbfNodeReader1 {

	enum NodeType {
		NodeNone = 0,
		StartObject = 1,
		EndObject = 2,
		StartProperty = 3,
		EndProperty = 4,
		Text = 5,
		Value = 6,
		Namespace = 7,
		EndOfAttributes = 8,
		EndOfStream = 9,
		LineInfo = 10,
		LineInfoAbsolute = 11
	};

	XbfContext							&context;
	dynamic_array<XamlObjectNode*>		typeStack;
	dynamic_array<XamlPropertyNode*>	propertyStack;

	XamlObjectNode	*root;

	XbfNodeReader1(XbfContext &_context) : context(_context), root(0) {}

	void	skipLineInfo(istream_ref file, int lineNumber, int linePosition) {
		auto lineNumberDelta = file.get<uint16>();
		auto linePositionDelta = file.get<int16>();
	}

	void	skipLineInfoAbsolute(istream_ref file) {
		auto lineNumber = file.get<uint32>();
		auto linePosition = file.get<uint32>();
	}

	bool	read(istream_ref file, streamptr end) {
		auto currentLinePosition = 0;
		auto currentLineNumber = 0;

		while (file.tell() != end) {
			NodeType nodeType = (NodeType)file.getc();
			switch (nodeType) {
				case LineInfo:
					skipLineInfo(file, currentLineNumber, currentLinePosition);
					break;

				case LineInfoAbsolute:
					skipLineInfoAbsolute(file);
					break;

				case Namespace: {
					auto		ns		= context.xml_namespaces[file.get<uint32>()].name(context);
					uint32		flags	= file.get<uint32>();
					file.read(context.ns_prefixes[ns].put());
					break;
				}

				case StartObject: {
					auto	*node = new XamlObjectNode(file);
					if (root == nullptr)
						root = node;
					typeStack.push_back(node);
					break;
				}

				case EndObject:
					if (propertyStack.size() > 0) {
						auto parentProperty = propertyStack.back();
						auto node = typeStack.back();
						parentProperty->values.push_back(node);
					}
					typeStack.pop_back();
					break;

				case StartProperty:
					propertyStack.push_back(new XamlPropertyNode(file));
					break;

				case EndProperty:
					if (typeStack.size() > 0) {
						auto parentObject = typeStack.back();
						parentObject->properties.push_back(propertyStack.back());
					}
					propertyStack.pop_back();
					break;

				case Value: {
					auto	*node = new XamlValueNode(file);
					if (propertyStack.size() > 0) {
						auto parentProperty = propertyStack.back();
						parentProperty->values.push_back(node);
					}
					break;
				}
				case Text: {
					auto	*node = new XamlTextNode(file);
					if (propertyStack.size() > 0) {
						auto parentProperty = propertyStack.back();
						parentProperty->values.push_back(node);
					}
					break;
				}
				default:
					return false;
			}
		}
		return true;
	}
};

//-----------------------------------------------------------------------------
//	XBF v2.1
//-----------------------------------------------------------------------------

struct XbfValue {
	enum Type {
		Unknown			= 0,
		False			= 1,
		True			= 2,
		Float			= 3,
		Signed			= 4,
		SharedString	= 5,
		Border			= 6,
		GridLength		= 7,
		Color			= 8,
		String			= 9,
		NullString		= 10,
		StableIndex		= 11,
	};

	uint8	type;
	union {
		const void		*p;
		float			f;
		int32			i;
		XbfColor		color;
		XbfIndex		x;
		XbfString		s;
		XbfStableIndex	stable_index;
		XbfBorder		border;
		XbfGridLength	grid_length;
	};

	XbfValue()					{ clear(*this); }
	XbfValue(XbfValue &&b)		{ memcpy(this, &b, sizeof(*this)); b.type = Unknown; }

	XbfValue(bool					v)	: type(v ? True : False)	{}
	XbfValue(float					v)	: type(Float)		{ f = v; }
	XbfValue(int32					v)	: type(Signed)		{ i = v; }
	XbfValue(XbfColor				v)	: type(Color)		{ color = v; }
	XbfValue(XbfIndex				v)	: type(SharedString){ x = v; }
	XbfValue(const XbfString&		v)	: type(String)		{ clear(s); s = v; ; }
	XbfValue(XbfString&&			v)	: type(String)		{ clear(s); s = move(v); }
	XbfValue(const XbfBorder&		v)	: type(Border)		{ border = v; }
	XbfValue(const XbfGridLength&	v)	: type(GridLength)	{ grid_length = v; }

	XbfValue(const string&	v)	: type(String)	{ clear(s); (string16&)s = v; }
	XbfValue(const void*	v)	: type(Unknown)	{ p = v; }

	~XbfValue() {
		if (type == String)
			s.~XbfString();
	}

	void operator=(XbfValue &&b)	{ memcpy(this, &b, sizeof(*this)); clear(b); }

	bool	read(istream_ref file) {
		type	= file.getc();
		switch (type) {
			case False:
			case True:
			case NullString:	return true;
			case Float:			return file.read(f);
			case Signed:		return file.read(i);
			case SharedString:	return file.read(x);
			case Border:		return file.read(border);
			case GridLength:	return file.read(grid_length);
			case Color:			return file.read(color);
			case String:		return file.read(s);
			case StableIndex:	return file.read(stable_index);
			default:			return false;
		}
	}

	template<typename T> operator const T*() const {
		return type == Unknown ? (const T*)p : nullptr;
	}
	string16	as_string(const XbfContext &ctx) const {
		switch (type) {
			case NullString:	return 0;
			case String:		return s;
			case SharedString:	return ctx.strings[x.i];
			default:			return 0;
		}
	}

};

struct XbfNodeReader2 {
	enum NodeType {
		None													= 0x00,	//
		PushScope												= 0x01,	//
		PopScope												= 0x02,	//	-
		AddNamespace											= 0x03,	// uint16, string
		PushConstant											= 0x04,	//
		PushResolvedType										= 0x05,	//	+2
		PushResolvedProperty									= 0x06,	//
		SetValue												= 0x07,	//	+2
		AddToCollection											= 0x08,	//	-
		AddToDictionary											= 0x09,	//	-
		AddToDictionaryWithKey									= 0x0a,	//	value key
		CheckPeerType											= 0x0b,	//	string
		SetConnectionId											= 0x0c,	//	value
		SetName													= 0x0d,	//	value
		GetResourcePropertyBag									= 0x0e,	//	value
		SetCustomRuntimeData									= 0x0f,	//	+32
		SetResourceDictionaryItems								= 0x10,	//
		SetDeferredProperty										= 0x11,	//	uint16 prop, uintv nstatic, uintv ntheme, uint16[nstatic], uint16[ntheme],
		PushScopeAddNamespace									= 0x12,	//	uint16, string
		PushScopeGetValue										= 0x13,	//	+2
		PushScopeCreateTypeBeginInit							= 0x14,	//	+2
		PushScopeCreateTypeWithConstantBeginInit				= 0x15,	//	+7
		PushScopeCreateTypeWithTypeConvertedConstantBeginInit	= 0x16,	//	+13
		CreateTypeBeginInit										= 0x17,	//	+2
		CreateTypeWithConstantBeginInit							= 0x18,	//	+7
		CreateTypeWithTypeConvertedConstantBeginInit			= 0x19,	//	+13
		SetValueConstant										= 0x1a,	//	uint16, value
		SetValueTypeConvertedConstant							= 0x1b,	//	uint16; value
		SetValueTypeConvertedResolvedProperty					= 0x1c,	//
		SetValueTypeConvertedResolvedType						= 0x1d,	//	+4
		SetValueFromStaticResource								= 0x1e,	//	value
		SetValueFromTemplateBinding								= 0x1f,	//	uint16 property, uint16 binding_path
		SetValueFromMarkupExtension								= 0x20,	//	+2
		EndInitPopScope											= 0x21,	//	-
		ProvideStaticResourceValue								= 0x22,	//	value
		ProvideThemeResourceValue								= 0x23,	//	value
		SetValueFromThemeResource								= 0x24,	//	uint16, value
		EndOfStream												= 0x25,	//
		BeginConditionalScope									= 0x26,	//
		EndConditionalScope										= 0x27,	//
		CreateType												= 0x80,	//
		CreateTypeWithInitialValue								= 0x81,	//
		BeginInit												= 0x82,	//
		EndInit													= 0x83,	//
		GetValue												= 0x84,	//
		TypeConvertValue										= 0x85,	//
		PushScopeCreateType										= 0x86,	//
		PushScopeCreateTypeWithConstant							= 0x87,	//
		PushScopeCreateTypeWithTypeConvertedConstant			= 0x88,	//
		CreateTypeWithConstant									= 0x89,	//
		CreateTypeWithTypeConvertedConstant						= 0x8a,	//
		ProvideValue											= 0x8b,	//
		ProvideTemplateBindingValue								= 0x8c,	//
		SetDirectiveProperty									= 0x8d,	//
		StreamOffsetMarker										= 0x8e,	//
	};

	struct Object;
	struct Property {
		enum {
			ThemeResource	= 1,
			StaticResource	= 2,
			Binding			= 4,
			Converted		= 8,
			IsObject		= 16,
		};
		string16	name;
		XbfValue	value;
		uint32		flags;
		Property() {}
		Property(const string16 &_name, XbfValue &&_value, uint32 _flags = 0) : name(_name), value(move(_value)), flags(_flags) {}
		Property(const string16 &_name, Object *_obj, uint32 _flags = 0) : name(_name), value(_obj), flags(_flags | IsObject) {}
    };
	struct Object {
		string16	type;
		XbfValue	name;
		dynamic_array<Property> properties;
		dynamic_array<Object*>	children;

		template<typename T> void add_property(string16 &&name, T &&t, uint32 flags = 0) {
			properties.emplace_back(move(name), forward<T>(t), flags);
		}
	};

	struct ObjectStack : dynamic_array<Object*> {
		Object *push(Object *obj) {
			push_back(obj);
			return new Object;
		}
		Object *pop() {
			return pop_back_value();
		}
	};

	XbfContext	&context;

	XbfNodeReader2(XbfContext &_context) : context(_context) {}

	string GetTypeName(uint16 id) {
		if (id & 0x8000) {
			if (auto *name = XbfBuiltin::GetType(id & ~0x8000))
				return name;
			return format_string("UnknownType0x%04x", id);
		}

		auto	type	= context.types[id];
		auto	ns		= "using:" + type.namespce(context).name(context);
		return context.ns_prefixes.check(ns)
			? context.ns_prefixes[ns].put() + ":" + type.name(context)
			: type.name(context);
	}

	string GetPropertyName(int id) {
		if (id & 0x8000) {
			if (auto *name = XbfBuiltin::GetProperty(id & ~0x8000))
				return name;
			return format_string("UnknownProperty0x%04x", id);
		}
		return context.properties[id].name(context);
	}

	streamptr	begin_section(istream_ref file, XbfContext::Section &section, int offset = 0) {
		file.seek(context.data_start + section.nodes + offset);
		return context.data_start + section.lines;
	}

	void begin_init() {
	}

	Object	*read_object(istream_ref file, streamptr end) {
		ObjectStack		scope;
		Object			*obj	= 0;
		Object			*value	= 0;

		while (file.tell() < end) {
			NodeType nodeType = (NodeType)file.getc();
			switch (nodeType) {
				case None:
				case PushConstant:
				case PushResolvedType:
				case PushResolvedProperty:
				case SetResourceDictionaryItems:
				case SetValueTypeConvertedResolvedProperty:
				case EndOfStream:
				case BeginConditionalScope:
				case EndConditionalScope:
				case CreateTypeWithInitialValue:
				case TypeConvertValue:
				case ProvideTemplateBindingValue:
				case SetDirectiveProperty:
				case StreamOffsetMarker:
					return nullptr;

				case PushScope:
					obj = scope.push(obj);
					break;

				case PushScopeCreateType:
					obj = scope.push(obj);
				case CreateType:
					return nullptr;

				case PushScopeCreateTypeWithConstant:
					obj = scope.push(obj);
				case CreateTypeWithConstant:
					return nullptr;

				case PushScopeCreateTypeWithTypeConvertedConstant:
					obj = scope.push(obj);
				case CreateTypeWithTypeConvertedConstant:
					return nullptr;

				case PopScope:
					obj = scope.pop();
					break;

				case PushScopeAddNamespace:
					obj = scope.push(obj);
				case AddNamespace: {
					auto	ns = context.xml_namespaces[file.get<uint16>()].name(context);
					file.read(context.ns_prefixes[ns].put());
					break;
				}

				case SetValueFromMarkupExtension:
				case SetValue: {
					auto	property_name = GetPropertyName(file.get<uint16>());
					obj->add_property(property_name, value);
					break;
				}

				case AddToCollection:
				case AddToDictionary:
					obj->children.push_back(value);
					break;

				case AddToDictionaryWithKey: {
					file.read(value->name);
					obj->children.push_back(value);
					break;
				}

				case CheckPeerType:
					obj->add_property("x:Class", file.get<XbfString>());
					break;

				case SetConnectionId:
					obj->add_property("ConnectionId", file.get<XbfValue>());
					break;

				case SetName:
					file.read(obj->name);
					break;

				case SetCustomRuntimeData: {
					read_section_reference(obj, file);
					//char	unknown[31];
					//file.read(unknown);
					break;
				}
				case GetResourcePropertyBag:
					obj->add_property("PropertyBag", file.get<XbfValue>());
					break;

				case SetDeferredProperty: {
					read_data_template(obj, file);
					//char	unknown[4];
					//file.read(unknown);
					break;
				}
				case PushScopeGetValue:
					obj = scope.push(obj);
				case GetValue: {
					auto	property_name = GetPropertyName(file.get<uint16>());
					scope.back()->add_property(property_name, obj);
					break;
				}
				case PushScopeCreateTypeWithConstantBeginInit:
					obj = scope.push(obj);
				case CreateTypeWithConstantBeginInit:
					obj->type	= GetTypeName(file.get<uint16>());
					obj->add_property("Value", file.get<XbfValue>());
				case BeginInit:
					begin_init();
					break;

				case PushScopeCreateTypeWithTypeConvertedConstantBeginInit:
					obj = scope.push(obj);
				case CreateTypeWithTypeConvertedConstantBeginInit: {
					auto	type_name = GetTypeName(file.get<uint16>());
					obj->add_property("x:Class", type_name);
					obj->add_property("Value", file.get<XbfValue>());
					begin_init();
					break;
				}
				case PushScopeCreateTypeBeginInit:
					obj = scope.push(obj);
				case CreateTypeBeginInit:
					obj->type	= GetTypeName(file.get<uint16>());
					begin_init();
					break;

				case SetValueConstant: {
					auto	property_name = GetPropertyName(file.get<uint16>());
					obj->add_property(property_name, file.get<XbfValue>());
					break;
				}
				case SetValueTypeConvertedConstant: {
					auto	property_name = GetPropertyName(file.get<uint16>());
					obj->add_property(property_name, file.get<XbfValue>(), Property::Converted);
					break;
				}
				case SetValueTypeConvertedResolvedType: {
					auto	property_name = GetPropertyName(file.get<uint16>());
					obj->add_property(property_name, GetTypeName(file.get<uint16>()));
					break;
				}
				case SetValueFromStaticResource: {
					auto	property_name = GetPropertyName(file.get<uint16>());
					obj->add_property(property_name, file.get<XbfValue>(), Property::StaticResource);
					break;
				}
				case SetValueFromTemplateBinding: {
					auto	property_name	= GetPropertyName(file.get<uint16>());
					auto	binding_path	= GetPropertyName(file.get<uint16>());
					obj->add_property(property_name, file.get<XbfValue>(), Property::Binding);
					break;
				}
				case EndInit:
					return obj;

				case EndInitPopScope:
					if (scope.size() == 1)
						return obj;
					value	= obj;
					obj		= scope.pop();
					break;

				case ProvideStaticResourceValue:
					obj->type = "StaticResource";
					obj->add_property("ResourceKey", file.get<XbfValue>(), Property::StaticResource);
					return obj;

				case ProvideThemeResourceValue:
					obj->type = "ThemeResource";
					obj->add_property("ResourceKey", file.get<XbfValue>(), Property::ThemeResource);
					return obj;

				case SetValueFromThemeResource: {
					auto	property_name = GetPropertyName(file.get<uint16>());
					obj->add_property(property_name, file.get<XbfValue>(), Property::ThemeResource);
					break;
				}
				case ProvideValue:
					return obj;

			}
		}
		return obj;
	}

	Object *read_object_in_section(istream_ref file, XbfContext::Section &section, int offset) {
		streamptr	old_pos	= file.tell();
		Object		*obj	= read_object(file, begin_section(file, section, offset));
		file.seek(old_pos);
		return obj;
	}

	bool SkipVisualStateBytes(istream_ref file) {
		// Number of visual states
		// The following bytes indicate which visual states belong in each group
		dynamic_array<int> visualStateGroupMemberships((int)file.get<leb128<uint32>>());
		for (auto &i : visualStateGroupMemberships)
			i = file.get<leb128<uint32>>();

		// Number of visual states (again?)
		// Get the VisualState objects
		dynamic_array<Object*>	visualStates((int)file.get<leb128<uint32>>());
		for (auto &state : visualStates) {
			int	name_id = file.get<uint16>();

			(void)file.get<leb128<uint32>>(); // TODO: purpose unknown
			(void)file.get<leb128<uint32>>(); // TODO: purpose unknown

			// Get the Setters for this VisualState
			for (int j = 0, n = file.get<leb128<uint32>>(); j < n; j++)
				int setter_offset = file.get<leb128<uint32>>();

			// Get the AdaptiveTriggers for this VisualState
			for (int j = 0, n = file.get<leb128<uint32>>(); j < n; j++) {
				// I'm not sure what this second count is for -- possibly for the number of properties set on the trigger
				int count = file.get<leb128<uint32>>();
				for (int k = 0; k < count; k++)
					file.get<leb128<uint32>>(); // TODO: purpose unknown
			}

			// Get the StateTriggers for this VisualState
			for (int j = 0, n = file.get<leb128<uint32>>(); j < n; j++)
				int stateTriggerOffset = file.get<leb128<uint32>>();

			// Always 0 or 2
			for (int j = 0, n = file.get<leb128<uint32>>(); j < n; j++)
				int offset = file.get<leb128<uint32>>(); // Secondary node stream offset of StateTriggers and Setters collection

			if (file.get<leb128<uint32>>() != 0) // TODO: purpose unknown
				return false;
		}

		// Get the VisualStateGroup objects
		dynamic_array<Object*>	visualStateGroups((int)file.get<leb128<uint32>>());
		for (auto &group : visualStateGroups) {
			int		name_id = file.get<uint16>();

			(void)file.get<leb128<uint32>>(); // TODO, always 1 or 2

			// The offset within the node section for this VisualStateGroup
			int object_offset = file.get<leb128<uint32>>();

			group = new Object;
			group->type = "VisualStateGroup";
			group->name = context.strings[name_id];

			// Get the visual states that belong to this group
			dynamic_array<Object*>	states;
			int	i = visualStateGroups.index_of(group);
			for (auto &j : visualStateGroupMemberships) {
				if (j == i)
					states.push_back(visualStates[j]);
			}
			if (!states.empty())
				group->add_property("States", "dummy");
		}

		for (int i = 0, n = file.get<leb128<uint32>>(); i < n; i++) {
			auto	toState		= context.strings[file.get<uint16>()];
			auto	fromState	= context.strings[file.get<uint16>()];
			int		offset		= file.get<leb128<uint32>>();
		}

		(void)file.get<leb128<uint32>>(); // TODO: always 1 or 2

		for (int i = 0, n = file.get<leb128<uint32>>(); i < n; i++) {
			int index1 = file.get<leb128<uint32>>(); // Visual state index or -1
			int index2 = file.get<leb128<uint32>>(); // Visual state index or -1
			file.get<leb128<uint32>>();
		}

		for (int i = 0, n = file.get<leb128<uint32>>(); i < n; i++)
			file.get<leb128<uint32>>();

		(void)file.get<leb128<uint32>>(); // TODO: unknown purpose

		// At the end we have a list of string references
		for (int i = 0, n = file.get<leb128<uint32>>(); i < n; i++)
			string str = context.strings[file.get<uint16>()];

		// At this point we have a list of VisualStateGroup objects in the visualStateGroups variable.
		// These could be added to the result, but we already have them there from parsing the specified node section.
		return true;
	}

	bool read_data_template(Object *obj, istream_ref file) {
		string	property_name	= GetPropertyName(file.get<uint16>()); // Always "Template"
		auto	section			= context.sections[file.get<leb128<uint32>>()];

		// List of StaticResources and ThemeResources referenced from inside the DataTemplate
		int static_count	= file.get<leb128<uint32>>();
		int theme_count		= file.get<leb128<uint32>>();

		for (int i = 0; i < static_count; i++)
			auto resource = context.strings[file.get<uint16>()];

		for (int i = 0; i < theme_count; i++)
			auto resource = context.strings[file.get<uint16>()];

		streamptr	old_pos	= file.tell();

		auto	end		= begin_section(file, section);
		while (auto child = read_object(file, end))
			obj->add_property(property_name, child);

		file.seek(old_pos);
		return true;
	}

	bool read_resource_dictionary(dynamic_array<Object*> &objects, istream_ref file, XbfContext::Section section, bool extended) {
		// Resources with keys
		for (int i = 0, n = file.get<leb128<uint32>>(); i < n; i++) {
			auto	key		= context.strings[file.get<uint16>()];
			Object	*child	= read_object_in_section(file, section, file.get<leb128<uint32>>());
			child->name		= key;
			objects.push_back(child);
		}

		// A subset of the resource keys from above seem to get repeated here, purpose unknown
		for (int i = 0, n = file.get<leb128<uint32>>(); i < n; i++)
			auto key = context.strings[file.get<uint16>()];

		// Styles with TargetType and no key
		for (int i = 0, n = file.get<leb128<uint32>>(); i < n; i++) {
			auto	target	= context.strings[file.get<uint16>()];
			Object	*child	= read_object_in_section(file, section, file.get<leb128<uint32>>());
			objects.push_back(child);
		}

		if (extended && file.get<leb128<uint32>>() != 0) // TODO: purpose unknown
			return false;

		// A subset of the target types from above seem to get repeated here, purpose unknown
		for (int i = 0, n = file.get<leb128<uint32>>(); i < n; i++)
			auto target = context.strings[file.get<uint16>()];
		return true;
	}

	bool read_style(Object *obj, istream_ref file, XbfContext::Section section) {
		enum Flags {
			ThemeResource	= 1,
			StaticResource	= 2,
            General			= 8,
			IsProperty		= 0x10,
			Constant		= 0x20,
		};

		int		type		= file.getc();
		int		name_id		= file.get<uint16>();
		auto	name		= type & IsProperty ? GetPropertyName(name_id) : context.strings[name_id];
		auto	type_name	= !(type & IsProperty) ? GetTypeName(file.get<uint16>()) : 0;

		if (type & Constant) {
			obj->add_property(name, file.get<XbfValue>(), type & 3);
			return true;
		}

		if (auto *child = read_object_in_section(file, section, file.get<leb128<uint32>>())) {
			obj->add_property(name, child, type & 3);
			return true;
		}

		return false;
	}

	bool read_section_reference(Object *obj, istream_ref file) {
		// The node section we're skipping to
		XbfContext::Section section = context.sections[file.get<leb128<uint32>>()];
		uint16	id	= file.get<uint16>();	//unknown purpose = 0

		// Get the type of nodes contained in this section
		uint32	type = file.get<leb128<uint32>>();
		switch (type) {
			case 2: // Style
			case 8: // 8 seems to be equivalent
				for (int i = 0, n = file.get<leb128<uint32>>(); i < n; i++) {
					if (!read_style(obj, file, section))
						return false;
				}
				return true;

			case 7: // ResourceDictionary
				return read_resource_dictionary(obj->children, file, section, false);

			case 371: // ResourceDictionary
				return read_resource_dictionary(obj->children, file, section, true);

			case 5: {// Visual states
				SkipVisualStateBytes(file);
				auto	end		= begin_section(file, section);
				while (auto child = read_object(file, end))
					obj->children.push_back(child);
				return true;
			}
			case 6: {// DeferredElement
				// The following properties can be ignored as they will appear in the secondary node section again
				for (int i = 0, n = file.get<leb128<uint32>>(); i < n; i++) {
					string property_name = GetPropertyName(file.get<uint16>());
					file.get<XbfValue>();
				}
				auto	name	= context.strings[file.get<uint16>()];
				auto	end		= begin_section(file, section);
				while (auto child = read_object(file, end))
					obj->children.push_back(child);
				return true;
			}
			case 746: {// DeferredElement
				auto	name	= context.strings[file.get<uint16>()];
				auto	end		= begin_section(file, section);
				while (auto child = read_object(file, end))
					obj->children.push_back(child);
				return true;
			}
			case 10: {
				int n = file.get<leb128<uint32>>();
				for (int i = 0; i < n; i++) {
					uint16	id		= file.get<uint16>();
					Object	*child	= read_object_in_section(file, section, file.get<leb128<uint32>>());
					child->name		= context.strings[id];
					obj->children.push_back(child);
				}
				char	unknown[5];
				file.read(unknown);
				return true;
			}
			default:
				return false;
		}
	}

};

XbfValue ToV2(XamlValueNode *v) {
	switch (v->id) {
//		case XamlValueNode::None:
		case XamlValueNode::False:			return false;
		case XamlValueNode::True:			return true;
		case XamlValueNode::Float:			return v->f;
		case XamlValueNode::Signed:			return v->i;
		case XamlValueNode::String:			return v->s;
//		case XamlValueNode::KeyTime:		return false;
		case XamlValueNode::Thickness:		return v->border;
//		case XamlValueNode::LengthConverter:return false;
		case XamlValueNode::GridLength:		return v->grid_length;
		case XamlValueNode::Color:			return v->color;
//		case XamlValueNode::Duration:		return false;
		default: return XbfValue();
	};
}

XbfNodeReader2::Object	*ToV2(const XbfContext &ctx, XamlObjectNode *obj) {
	XbfNodeReader2::Object	*obj2 = new XbfNodeReader2::Object;
	auto	ns = obj->namespce(ctx);

	if (auto *p = ctx.ns_prefixes.check("using:" + ns))
		obj2->type = *p + ":" + obj->type(ctx);
	else
		obj2->type = obj->type(ctx);

	for (auto n : obj->properties) {
		switch (n->type) {
			case XamlNode::Text:
			case XamlNode::Value: {
				obj2->add_property(0, ToV2((XamlValueNode*)n));
				break;
			}
			case XamlNode::Object: {
				XbfNodeReader2::Object	*o = ToV2(ctx, (XamlObjectNode*)n);
				obj2->add_property(0, o);
				break;
			}
			case XamlNode::Property: {
				auto	*p = (XamlPropertyNode*)n;
				if (p->values.size() == 1) {
					switch (p->values[0]->type) {
						case XamlNode::Value:
							obj2->add_property(p->name(ctx), ToV2((XamlValueNode*)p->values[0]));
							break;
						case XamlNode::Object:
							obj2->add_property(p->name(ctx), ToV2(ctx, (XamlObjectNode*)p->values[0]));
							break;
					}
					break;
				}
				XbfNodeReader2::Object	*o = new XbfNodeReader2::Object;
				for (auto v : p->values) {
					if (v->type == XamlNode::Object)
						o->children.push_back(ToV2(ctx, (XamlObjectNode*)v));
				}
				obj2->add_property(p->name(ctx), o);
				break;

			}
		}
	}
	return obj2;
}


struct XbfReader : XbfHeader, XbfContext {

	XbfReader(istream_ref file) { file.read(*(XbfHeader*)this); }
	bool		validate() { return MagicNumber[0] == 'X' && MagicNumber[1] == 'B' && MagicNumber[2] == 'F' && MagicNumber[3] == 0; }
	dynamic_array<XbfNodeReader2::Object*>	objects;

	bool	read(istream_ref file) {
		strings.resize(file.get<uint32>());
		for (auto &i : strings) {
			file.read(i);
			if (MajorFileVersion > 1)
				file.get<uint16>();
		}

		assemblies.read(file, file.get<uint32>());
		namespaces.read(file, file.get<uint32>());
		types.read(file, file.get<uint32>());
		properties.read(file, file.get<uint32>());
		xml_namespaces.read(file, file.get<uint32>());

		if (MajorFileVersion == 1) {
			XbfNodeReader1	reader(*this);
			if (!reader.read(file, file.tell() + NodeSize))
				return false;
			objects.push_back(ToV2(*this, reader.root));
			return true;
		}

		if (MajorFileVersion == 2) {
			sections.read(file, file.get<uint32>());
			data_start = file.tell();
#if 1
			XbfNodeReader2	reader(*this);
			streamptr		end = reader.begin_section(file, sections.front());
			while (auto obj = reader.read_object(file, end))
				objects.push_back(obj);
#else
			for (auto &i : sections) {
				XbfNodeReader2	reader(*this);
				streamptr	end = reader.begin_section(file, i);
				dynamic_array<XbfNodeReader2::Object*>	objects;
				while (auto obj = reader.read_object(file, end))
					objects.push_back(obj);
			}
#endif
			return true;
		}

		return false;
	}
};

ISO_ptr<void> PropToISO(const XbfContext &ctx, const XbfNodeReader2::Property &prop);

ISO_ptr<void> ToISO(const XbfContext &ctx, const XbfNodeReader2::Object *obj) {
	ISO_ptr<anything>	p(str8(obj->name.as_string(ctx)));
	p->Append(ISO::MakePtr("type", obj->type));
	if (obj->properties) {
		ISO_ptr<anything>	props("properties");
		p->Append(props);
		for (auto &i : obj->properties)
			props->Append(PropToISO(ctx, i));
	}
	for (auto i : obj->children)
		p->Append(ToISO(ctx, i));
	return p;
}

ISO_DEFUSERCOMPV(XbfBorder, left, top, right, bottom);
ISO_DEFUSERCOMPV(XbfGridLength, type, value);

ISO_ptr<void> PropToISO(const XbfContext &ctx, const XbfNodeReader2::Property &prop) {
	tag2	id = str8(prop.name);

	if (prop.flags & XbfNodeReader2::Property::IsObject) {
		auto	p = ToISO(ctx, prop.value);
		p.SetID(id);
		return p;
	}

	switch (prop.value.type) {
//		case XbfValue::Unknown:
		case XbfValue::False:			return ISO::MakePtr(id, false);
		case XbfValue::True:			return ISO::MakePtr(id, true);
		case XbfValue::Float:			return ISO::MakePtr(id, prop.value.f);
		case XbfValue::Signed:			return ISO::MakePtr(id, prop.value.i);
		case XbfValue::SharedString:	return ISO::MakePtr(id, prop.value.as_string(ctx));
		case XbfValue::Border:			return ISO::MakePtr(id, prop.value.border);
		case XbfValue::GridLength:		return ISO::MakePtr(id, prop.value.grid_length);
		case XbfValue::Color:			return ISO::MakePtr(id, prop.value.color.i);
		case XbfValue::String:			return ISO::MakePtr<string16>(id, prop.value.as_string(ctx));
		case XbfValue::NullString:		return ISO::MakePtr<string16>(id, nullptr);
		case XbfValue::StableIndex:		return ISO::MakePtr(id, XbfBuiltin::GetEnum(prop.value.stable_index.index, prop.value.stable_index.value));
		default: return ISO_NULL;
	}
}

class XBFFileHandler : FileHandler {
	const char*		GetExt() override { return "xbf"; }
	const char*		GetDescription() override { return "Compiled XAML"; }

	int					Check(istream_ref file) override {
		file.seek(0);
		XbfReader	r(file);
		return  r.validate() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		//HMODULE		h	= LoadLibrary("C:\\Program Files (x86)\\Windows Kits\\10\\bin\\10.0.17115.0\\x64\\genxbf.dll");
		XbfReader	r(file);
		if (!r.validate())
			return ISO_NULL;
		if (r.read(file)) {
			if (r.objects.size() == 1)
				return ToISO(r, r.objects.front());
			ISO_ptr<anything>	p(id);
			for (auto i : r.objects)
				p->Append(ToISO(r, i));
			return p;
		}
		file.seek(0);
		return ISO_NULL;
	}
} xbf;