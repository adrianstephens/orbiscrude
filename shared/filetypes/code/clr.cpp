#include "clr.h"

namespace clr {

template<> uint8 CodedIndex<_TypeDefOrRef>::trans[]			= {TypeDef, TypeRef, TypeSpec};
template<> uint8 CodedIndex<_HasConstant>::trans[]			= {Field, Param, Property};
template<> uint8 CodedIndex<_HasCustomAttribute>::trans[]	= {
	MethodDef, Field, TypeRef, TypeDef, Param, InterfaceImpl, 
	MemberRef, Module, DeclSecurity, Property, Event, StandAloneSig, 
	ModuleRef, TypeSpec, Assembly, AssemblyRef, File, ExportedType, 
	ManifestResource, GenericParam, GenericParamConstraint, MethodSpec
};
template<> uint8 CodedIndex<_HasFieldMarshall>::trans[]		= {Field, Param};
template<> uint8 CodedIndex<_HasDeclSecurity>::trans[]		= {TypeDef, MethodDef, Assembly};
template<> uint8 CodedIndex<_MemberRefParent>::trans[]		= {TypeDef, TypeRef, ModuleRef, MethodDef, TypeSpec};
template<> uint8 CodedIndex<_HasSemantics>::trans[]			= {Event, Property};
template<> uint8 CodedIndex<_MethodDefOrRef>::trans[]		= {MethodDef, MemberRef};
template<> uint8 CodedIndex<_MemberForwarded>::trans[]		= {Field, MethodDef};
template<> uint8 CodedIndex<_Implementation>::trans[]		= {File, AssemblyRef, ExportedType};
template<> uint8 CodedIndex<_CustomAttributeType>::trans[]	= {0, 0, MethodDef, MemberRef};
template<> uint8 CodedIndex<_ResolutionScope>::trans[]		= {Module, ModuleRef, AssemblyRef, TypeRef};
template<> uint8 CodedIndex<_TypeOrMethodDef>::trans[]		= {TypeDef, MethodDef};

}	//namespace clr



