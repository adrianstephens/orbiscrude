#ifndef DX11_EFFECT_H
#define DX11_EFFECT_H

#include "base/defs.h"
#include "base/array.h"

namespace iso { namespace dx11_effect {

// File format:
//   Header
//   Unstructured data block (BYTE[Header.cbUnstructured))
//   Structured data block
//     ConstantBuffer (ConstantBuffer CB) * Header.Effect.cCBs
//       uint32  num_annotations
//       Annotation data (Annotation) * (num_annotations) *this structure is variable sized
//       Variable data (NumericVariable Var) * (CB.cVariables)
//         uint32  num_annotations
//         Annotation data (Annotation) * (num_annotations) *this structure is variable sized
//     Object variables (ObjectVariable Var) * (Header.cObjectVariables) *this structure is variable sized
//       uint32  num_annotations
//       Annotation data (Annotation) * (num_annotations) *this structure is variable sized
//     Interface variables (InterfaceVariable Var) * (Header.cInterfaceVariables) *this structure is variable sized
//       uint32  num_annotations
//       Annotation data (Annotation) * (num_annotations) *this structure is variable sized
//     Groups (Group Group) * Header.cGroups
//       uint32  num_annotations
//       Annotation data (Annotation) * (num_annotations) *this structure is variable sized
//       Techniques (Technique Technique) * Group.cTechniques
//         uint32  num_annotations
//         Annotation data (Annotation) * (num_annotations) *this structure is variable sized
//         Pass (Pass Pass) * Technique.cPasses
//           uint32  num_annotations
//           Annotation data (Annotation) * (num_annotations) *this structure is variable sized
//           Pass assignments (Assignment) * Pass.cAssignments
	
//-----------------------------------------------------------------------------
//	helper templates
//-----------------------------------------------------------------------------

template<typename T> struct ptr {
	uint32		offset;
	const T*	get(const void *base)	const	{ return offset ? (T*)((uint8*)base + offset) : (T*)0; }
};

typedef ptr<const char>	ostring;

template<typename T> struct offset_element_base {
	const T		*p;
	const void	*base;
	offset_element_base() : p(0), base(0) {}
	offset_element_base(const T *_p, const void *_base) : p(_p), base(_base) {}
	operator const T&()		const	{ return *p; }
	const T* operator->()	const	{ return p; }
};

template<typename T> struct offset_element : offset_element_base<T> {
	offset_element() {}
	offset_element(const T *_t, const void *_base) : offset_element_base<T>(_t,_base) {}
};

template<typename T> const T* next(const T *p, const void *base) {
	return p + 1;
}

template<typename T> struct offset_container {
	uint32			num;
	const T			*start;
	const void		*base;

	struct iterator : offset_element<T> {
		uint32		index;
		iterator(uint32 n) : index(n)	{}
		iterator(const T *_p, const void *_base) : offset_element<T>(_p, _base), index(0)	{}
		iterator&			operator++()			{ p = next(p, this->base); ++index; return *this; };
		bool operator==(const iterator &b)	const	{ return index == b.index; };
		bool operator!=(const iterator &b)	const	{ return index != b.index; };
	};

	iterator	begin()		const { return iterator(start, base); }
	iterator	end()		const { return num; }
	const void	*find_end()	const {
		iterator	i = begin();
		for (uint32 n = num; n--; ++i);
		return i.p;
	}
	offset_container(const T *_start, uint32 _num, const void *_base) : num(_num), start(_start), base(_base) {}
};

//-----------------------------------------------------------------------------
//	binary structures
//-----------------------------------------------------------------------------

struct Header {
	enum TAG {
		fx_4_0 = 0xFEFF1001,
		fx_4_1 = 0xFEFF1011,
		fx_5_0 = 0xFEFF2001,
	};

	struct VarCounts {
		uint32	num_cb;
		uint32	num_numeric;
		uint32	num_obj;
	};

	uint32		tag;

	VarCounts	effect;
	VarCounts	pool;
	
	uint32		num_techniques;
	uint32		size_unstructured;

	uint32		num_strings;
	uint32		num_resources;

	uint32		num_DepthStencilBlocks;
	uint32		num_BlendStateBlocks;
	uint32		num_RasterizerStateBlocks;
	uint32		num_samplers;
	uint32		num_RenderTargetViews;
	uint32		num_DepthStencilViews;

	uint32		num_shaders;
	uint32		num_inline; // of the aforementioned shaders, the number that are defined inline within pass blocks
};

struct Header5 : public Header {
	uint32	num_groups;
	uint32	num_unordered_access_views;
	uint32	num_interface;
	uint32	num_interface_elements;
	uint32	num_instance_elements;
};

struct Type {
	enum TYPE {
		TT_Invalid,
		TT_Numeric,
		TT_Object,
		TT_Struct,
		TT_Interface,
	};
	enum OBJECT {
		OT_Invalid,						OT_String,					OT_Blend,						OT_DepthStencil,
		OT_Rasterizer,					OT_PixelShader,				OT_VertexShader,				OT_GeometryShader,
		OT_GeometryShaderSO,			OT_Texture,					OT_Texture1D,					OT_Texture1DArray,
		OT_Texture2D,					OT_Texture2DArray,			OT_Texture2DMS,					OT_Texture2DMSArray,
		OT_Texture3D,					OT_TextureCube,				OT_ConstantBuffer,				OT_RenderTargetView,
		OT_DepthStencilView,			OT_Sampler,					OT_Buffer,						OT_TextureCubeArray,
		OT_Count,						OT_PixelShader5,			OT_VertexShader5,				OT_GeometryShader5,
		OT_ComputeShader5,				OT_HullShader5,				OT_DomainShader5,				OT_RWTexture1D,
		OT_RWTexture1DArray,			OT_RWTexture2D,				OT_RWTexture2DArray,			OT_RWTexture3D,
		OT_RWBuffer,					OT_ByteAddressBuffer,		OT_RWByteAddressBuffer,			OT_StructuredBuffer,
		OT_RWStructuredBuffer,			OT_RWStructuredBufferAlloc,	OT_RWStructuredBufferConsume,	OT_AppendStructuredBuffer,
		OT_ConsumeStructuredBuffer,
	};

	ostring		name;
	TYPE		type;
	uint32		num_elements;	// # of array elements (0 for non-arrays)
	uint32		total_size;		// Size in bytes; not necessarily Stride * Elements for arrays because of possible gap left in final register
	uint32		stride;			// If an array, this is the spacing between elements. For unpacked arrays, always divisible by 16-bytes (1 register). No support for packed arrays
	uint32		packed_size;	// Size, in bytes, of this data typed when fully packed

	struct Struct {
		uint32		num;
		struct Member 	{
			ostring		name;
			ostring		semantic;
			uint32		offset;		// Offset, in bytes, relative to start of parent structure
			ptr<Type>	type;
		} array[1];
		struct Inheritance {
			ptr<Type>	base;
			uint32		num_interfaces;
			ptr<Type>	interfaces[1];
		};
	};

	struct Numeric {
		enum LAYOUT {
			NL_Invalid,
			NL_Scalar,
			NL_Vector,
			NL_Matrix,
		};
		enum TYPE {
			ST_Invalid,
			ST_Float,
			ST_Int,
			ST_UInt,
			ST_Bool,
		};
		LAYOUT	layout		: 3;
		TYPE	type		: 5;
		uint32	rows		: 3;	// 1 <= Rows <= 4
		uint32	cols		: 3;	// 1 <= Columns <= 4
		uint32	col_major	: 1;	// applies only to matrices
		uint32	packed		: 1;	// if this is an array, indicates whether elements should be greedily packed
	};

	union {
		Numeric		num;
		OBJECT		obj;
		Struct		strct;
	};

	bool IsObjectType(OBJECT target)	const { return type == TT_Object && obj == target; }
};

struct DataBlock {
	uint32	size;
	uint32	data[1];
};

struct Shader5 {
	struct Interface {
		ostring		name;
		uint32		array_index;
	};
	ptr<DataBlock>	shader;
	ostring			decls[4];
	uint32			num_decls;
	uint32			stream;			// Which stream is used for rasterization
	uint32			num_interface;
	ptr<Interface>	interfaces;
};

struct Assignment {
	enum TYPE {
		CAT_Invalid,			// Assignment-specific data (always in the unstructured blob)
		CAT_Constant,			// -N SConstant structures
		CAT_Variable,			// -NULL terminated string with variable name ("foo")
		CAT_ConstIndex,			// -SConstantIndex structure
		CAT_VariableIndex,		// -SVariableIndex structure
		CAT_ExpressionIndex,	// -SIndexedObjectExpression structure
		CAT_Expression,			// -DataBlock containing FXLVM code
		CAT_InlineShader,		// -Data block containing shader
		CAT_InlineShader5,		// -DataBlock containing shader with extended 5.0 data (ShaderData5)
	};
	enum STATE {
		//RObjects
		RasterizerState,		DepthStencilState,		BlendState,				RenderTargetView,
		DepthStencilView,		GenerateMips,
		//Shaders
		VertexShader,			PixelShader,			GeometryShader,
		//RObject config assignments
		DS_StencilRef,			AB_BlendFactor,			AB_SampleMask,
		//Raster
		FillMode,				CullMode,				FrontCounterClockwise,	DepthBias,
		DepthBiasClamp,			SlopeScaledDepthBias,	DepthClipEnable,		ScissorEnable,
		MultisampleEnable,		AntialiasedLineEnable,
		//Depth/Stencil
		DepthEnable,			DepthWriteMask,			DepthFunc,				StencilEnable,
		StencilReadMask,		StencilWriteMask,		FrontFaceStencilFail,	FrontFaceStencilDepthFail,
		FrontFaceStencilPass,	FrontFaceStencilFunc,	BackFaceStencilFail,	BackFaceStencilDepthFail,
		BackFaceStencilPass,	BackFaceStencilFunc,
		//Blend
		AlphaToCoverageEnable,	BlendEnable,			SrcBlend,				DestBlend,
		BlendOp,				SrcBlendAlpha,			DestBlendAlpha,			BlendOpAlpha,
		RenderTargetWriteMask,
		//Sampling
		Filter,					AddressU,				AddressV,				AddressW,
		MipLODBias,				MaxAnisotropy,			ComparisonFunc,			BorderColor,
		MinLOD,					MaxLOD,					Texture,
		//D3D11 
		HullShader,				DomainShader,			ComputeShader,
	};
	enum DEPTH_WRITE {
		NONE,				ALL
	};
	enum FILL {
		WIREFRAME,			SOLID
	};
	enum FILTER {
		MIN_MAG_MIP_POINT,						MIN_MAG_POINT_MIP_LINEAR,
		MIN_POINT_MAG_LINEAR_MIP_POINT,			MIN_POINT_MAG_MIP_LINEAR,
		MIN_LINEAR_MAG_MIP_POINT,				MIN_LINEAR_MAG_POINT_MIP_LINEAR,
		MIN_MAG_LINEAR_MIP_POINT,				MIN_MAG_MIP_LINEAR,
		ANISOTROPIC,							COMP_MIN_MAG_MIP_POINT,
		COMP_MIN_MAG_POINT_MIP_LINEAR,			COMP_MIN_POINT_MAG_LINEAR_MIP_POINT,
		COMP_MIN_POINT_MAG_MIP_LINEAR,			COMP_MIN_LINEAR_MAG_MIP_POINT,
		COMP_MIN_LINEAR_MAG_POINT_MIP_LINEAR,	COMP_MIN_MAG_LINEAR_MIP_POINT,
		COMP_MIN_MAG_MIP_LINEAR,				COMP_ANISOTROPIC,
		TEXT_1BIT,
	};
	enum BLEND {
		ZERO,				ONE,				SRC_COLOR,			INV_SRC_COLOR,
		SRC_ALPHA,			INV_SRC_ALPHA,		DEST_ALPHA,			INV_DEST_ALPHA,
		DEST_COLOR,			INV_DEST_COLOR,		SRC_ALPHA_SAT,		BLEND_FACTOR,
		INV_BLEND_FACTOR,	SRC1_COLOR,			INV_SRC1_COLOR,		SRC1_ALPHA,
		INV_SRC1_ALPHA,
	};
	enum TADDRESS {
		CLAMP,				WRAP,				MIRROR,				BORDER,			MIRROR_ONCE,
	};
	enum CULL {
		NOCULL,
		FRONT,
		BACK,
	};
	enum CMP {
		NEVER,				LESS,				EQUAL,				LESS_EQUAL,
		GREATER,			NOT_EQUAL,			GREATER_EQUAL,		ALWAYS,
	};
	enum STENCILOP {
		KEEP,				CLEAR,				REPLACE,			INCR_SAT,
		DECR_SAT,			INVERT,				INCR,				DECR,
	};
	enum BLENDOP {
		ADD,				SUBTRACT,			REV_SUBTRACT,		MIN,			MAX,
	};

	STATE		state;
	uint32		index;
	TYPE		type;
	ptr<void>	data;

	//CAT_Constant
	struct Constants {
		uint32	num_constants;
		struct Constant {
			Type::Numeric::TYPE type;
			union {
				bool	b;
				int		i;
				float	f;
			};
		} constants[1];
	};
	//CAT_ConstIndex
	struct ConstantIndex {
		ostring			array_name;
		uint32			index;
	};
	//CAT_VariableIndex
	struct VariableIndex {
		ostring			array_name;
		ostring			index_name;
	};
	//CAT_ExpressionIndex
	struct IndexedObjectExpression {	
		ostring			array_name;
		ptr<DataBlock>	code;
	};
	//CAT_InlineShader
	struct InlineShader {
		ptr<DataBlock>	shader;
		ostring			decl;
	};
};

struct Annotation {
	ostring			name;
	ptr<Type>		type;
	ptr<void>		defaults[1];
};

struct NumericVariable {
	ostring			name;			// Offset to variable name
	ptr<Type>		type;			// Offset to type information
	ostring			semantic;		// Offset to semantic information
	uint32			offset;			// Offset in parent constant buffer
	ptr<void>		def_val;		// Offset to default initializer value
	uint32			flags;			// Explicit bind point
};

struct ConstantBuffer {
	enum {TBuffer = 1 << 0, Single = 1 << 1};
	ostring			name;
	uint32			size;
	uint32			flags;
	uint32			num_variables;
	uint32			bind_point;			// Defined if the effect file specifies a bind point using the register keyword otherwise, -1
};

struct InterfaceVariable {
	ostring			name;			// Offset to variable name
	ptr<Type>		type;			// Offset to type information
	ptr<void>		def_val;		// Offset to default initializer array (SInterfaceInitializer[Elements])
	uint32			flags;
};

struct ObjectVariable {
	ostring			name;
	ptr<Type>		type;
	ostring			semantic;
	uint32			bind_point;			// -1 when not explicitly bound

	// OT_Blend, OT_DepthStencil, OT_Rasterizer, OT_Sampler
	struct State {
		uint32		num_assignments;
		Assignment	array[1];
		const State *next() const { return (const State*)(array + num_assignments); }
	};
	struct GSSO {
		ptr<DataBlock>	shader;
		ostring			decl;
	};

	union {
		uint32		data;
		ptr<void>	ptrs[1];	// OT_*Shader, OT_String: offsets to a shader data block or a NULL-terminated string
		State		blocks[1];	// OT_Blend, OT_DepthStencil, OT_Rasterizer, OT_Sampler
		GSSO		gsso[1];	// OT_GeometryShaderSO
		Shader5		shader5[1];	// For OBJECT == OT_*Shader5
	};
};

struct Group {
	ostring			name;
	uint32			num_techniques;
};

struct Technique {
	ostring			name;
	uint32			num_passes;
};

struct Pass {
	ostring			name;
	uint32			num_assignments;
};

//-----------------------------------------------------------------------------
//	parsing classes
//-----------------------------------------------------------------------------

typedef offset_container<Annotation>		Annotations;
typedef offset_container<NumericVariable>	NumericVariables;
typedef offset_container<ConstantBuffer>	ConstantBuffers;
typedef offset_container<ObjectVariable>	ObjectVariables;
typedef offset_container<InterfaceVariable>	InterfaceVariables;
typedef offset_container<Group>				Groups;
typedef offset_container<Technique>			Techniques;
typedef offset_container<Pass>				Passes;
typedef offset_container<Assignment>		Assignments;

struct AnnotationsHeader {
	uint32			num_annotations;
	Annotation		array[1];
	Annotations		GetAnnotations(const void *base)	const { return Annotations(array, num_annotations, base); }
};

template<typename T> struct Annotated : T, AnnotationsHeader {};

template<typename T> inline Annotations _GetAnnotations(const T *p, const void *base) {
	return static_cast<const Annotated<T>*>(p)->GetAnnotations(base);
}

inline Annotations _GetAnnotations(const ObjectVariable *p, const void *base) {
	const Type	*t	= p->type.get(base);
	uint32		n	= max(t->num_elements, 1);
	const void	*end;
	switch (t->obj) {
		default:
			end = &p->data;
			break;

	//StateBlock
		case Type::OT_Blend:
		case Type::OT_DepthStencil:
		case Type::OT_Rasterizer:
		case Type::OT_Sampler: {
			const ObjectVariable::State	*s = p->blocks;
			while (n--)
				s = s->next();
			end = s;
			break;
		}

	//Shader
		case Type::OT_String:
		case Type::OT_PixelShader:
		case Type::OT_VertexShader:
		case Type::OT_GeometryShader:
			end = &p->ptrs[n];
			break;

		case Type::OT_GeometryShaderSO:
			end = &p->gsso[n];
			break;

		case Type::OT_PixelShader5:
		case Type::OT_VertexShader5:
		case Type::OT_GeometryShader5:
		case Type::OT_ComputeShader5:
		case Type::OT_HullShader5:
		case Type::OT_DomainShader5:
			end = &p->shader5[n];
			break;
	}
	return ((AnnotationsHeader*)end)->GetAnnotations(base);
}

inline const Annotation* next(const Annotation *p, const void *base) {
	const Type	*t	= p->type.get(base);
	int			nd	= t->IsObjectType(Type::OT_String) ? max(t->num_elements, 1) : 1;
	return (Annotation*)(p->defaults + nd);
}
inline const NumericVariable* next(const NumericVariable *p, const void *base) {
	return (const NumericVariable*)_GetAnnotations(p, base).find_end();
}
inline const ObjectVariable* next(const ObjectVariable *p, const void *base) {
	return (const ObjectVariable*)_GetAnnotations(p, base).find_end();
}
inline const InterfaceVariable* next(const InterfaceVariable *p, const void *base) {
	return (const InterfaceVariable*)_GetAnnotations(p, base).find_end();
}
inline const ConstantBuffer* next(const ConstantBuffer *p, const void *base) {
	NumericVariables	v	= NumericVariables((NumericVariable*)_GetAnnotations(p, base).find_end(), p->num_variables, base);
	return (const ConstantBuffer*)v.find_end();
}
inline const Technique* next(const Technique *p, const void *base) {
	Passes				v	= Passes((Pass*)_GetAnnotations(p, base).find_end(), p->num_passes, base);
	return (const Technique*)v.find_end();
}
inline const Pass* next(const Pass *p, const void *base) {
	const Assignment	*a	= (const Assignment*)_GetAnnotations(p, base).find_end();
	return (const Pass*)(a + p->num_assignments);
}

template<> struct offset_element<Annotation> : offset_element_base<Annotation> {
	offset_element() {}
	offset_element(const Annotation *_t, const void *_base) : offset_element_base(_t,_base) {}
	const char *		GetName()			const	{ return p->name.get(base); }
};

template<> struct offset_element<ConstantBuffer> : offset_element_base<ConstantBuffer> {
	offset_element() {}
	offset_element(const ConstantBuffer *_t, const void *_base) : offset_element_base(_t,_base) {}
	Annotations			GetAnnotations()	const	{ return _GetAnnotations(p, base); }
	NumericVariables	GetVariables()		const	{ return NumericVariables((NumericVariable*)GetAnnotations().find_end(), p->num_variables, base); }
	const char *		GetName()			const	{ return p->name.get(base); }
};

template<> struct offset_element<Group> : offset_element_base<Group> {
	offset_element() {}
	offset_element(const Group *_t, const void *_base) : offset_element_base(_t,_base) {}
	Annotations			GetAnnotations()	const	{ return _GetAnnotations(p, base); }
	Techniques			GetTechniques()		const	{ return Techniques((Technique*)GetAnnotations().find_end(), p->num_techniques, base); }
	const char *		GetName()			const	{ return p->name.get(base); }
};

template<> struct offset_element<Technique> : offset_element_base<Technique> {
	offset_element() {}
	offset_element(const Technique *_t, const void *_base) : offset_element_base(_t,_base) {}
	Annotations			GetAnnotations()	const	{ return _GetAnnotations(p, base); }
	Passes				GetPasses()			const	{ return Passes((Pass*)GetAnnotations().find_end(), p->num_passes, base); }
	const char *		GetName()			const	{ return p->name.get(base); }
};

template<> struct offset_element<Pass> : offset_element_base<Pass> {
	offset_element() {}
	offset_element(const Pass *_t, const void *_base) : offset_element_base(_t,_base) {}
	Annotations			GetAnnotations()	const	{ return _GetAnnotations(p, base); }
	Assignments			GetAssignments()	const	{ return Assignments((Assignment*)GetAnnotations().find_end(), p->num_assignments, base); }
	const char *		GetName()			const	{ return p->name.get(base); }
};

struct Parser : Header5 {
	const void*			Base()					const { return this + 1; }
	const_memory_block	Unstructured()			const { return const_memory_block(this + 1, size_unstructured); }
	ConstantBuffers		GetConstantBuffers()	const { return ConstantBuffers((ConstantBuffer*)((uint8*)(this + 1) + size_unstructured), effect.num_cb, Base()); }
	ObjectVariables		GetObjectVariables()	const { return ObjectVariables((ObjectVariable*)GetConstantBuffers().find_end(), effect.num_obj, Base()); }
	InterfaceVariables	GetInterfaceVariables()	const { return InterfaceVariables((InterfaceVariable*)GetObjectVariables().find_end(), num_interface, Base()); }
	Groups				GetGroups()				const { return Groups((Group*)GetInterfaceVariables().find_end(), num_groups, Base()); }
};

} } // namespace iso::dx11_effect

#endif // DX11_EFFECT_H
