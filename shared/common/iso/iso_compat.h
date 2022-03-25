#ifndef ISO_COMPAT_H
#define ISO_COMPAT_H

#include "iso.h"

namespace iso {

template<class T> using ISO_weakref				= ISO::WeakRef<T>;
template<class T> using ISO_weakptr				= ISO::WeakPtr<T>;

template<int B> using ISO_allocate = ISO::allocate<B>;

using ISO::TypeEnumT;
using ISO::EnumT;

using ISO_weak				= ISO::Weak;
using ISO_TYPE				= ISO::TYPE;
using ISO_type				= ISO::Type;
using ISO_type_string		= ISO::TypeString;
using ISO_type_int			= ISO::TypeInt;
using ISO_type_float		= ISO::TypeFloat;
using ISO_type_string		= ISO::TypeString;
using ISO_type_composite	= ISO::TypeComposite;
using ISO_type_array		= ISO::TypeArray;
using ISO_type_openarray	= ISO::TypeOpenArray;
using ISO_type_reference	= ISO::TypeReference;
using ISO_type_user			= ISO::TypeUser;
using ISO_type_function		= ISO::TypeFunction;
using ISO_virtual			= ISO::Virtual;
using ISO_value				= ISO::Value;
using ISO_browser			= ISO::Browser;
using ISO_browser2			= ISO::Browser2;
using ISO_element			= ISO::Element;
using ISO_bitpacked			= ISO::BitPacked;
using ISO_virtual_defaults	= ISO::VirtualDefaults;
using ISO_enum				= ISO::Enum;

using ISO_openarray_header	= ISO::OpenArrayHead;
using CISO_type_int			= ISO::TypeInt;
using CISO_type_float		= ISO::TypeFloat;
using CISO_type_enum		= ISO::TypeEnum;
using CISO_type_composite	= ISO::TypeComposite;
using CISO_type_user		= ISO::TypeUserSave;
using CISO_type_user_comp	= ISO::TypeUserComp;
using CISO_type_callback	= ISO::TypeUserCallback;

using iso_string8			= ISO::ptr_string<char,32>;
using iso_void_ptr32		= ISO::void_ptr32;
using iso_void_ptr64		= ISO::void_ptr64;

using ISO::UserTypeArray;
template<typename T>	using TISO_accessor = ISO::AccessorT<T>;
using ISO::unescaped;

using ISO::_user_types;
using ISO::root;
using ISO::PtrBrowser;
using ISO::VStartBin;
using StartBin		= ISO::VStartBin<ISO::OpenArray<xint8, sizeof(void*) * 8> >;
using StartBinBlock	= ISO::VStartBin<const_memory_block>;

static const ISO::TYPE
	ISO_UNKNOWN		= ISO::UNKNOWN,
	ISO_INT			= ISO::INT,
	ISO_FLOAT		= ISO::FLOAT,
	ISO_STRING		= ISO::STRING,
	ISO_COMPOSITE	= ISO::COMPOSITE,
	ISO_ARRAY		= ISO::ARRAY,
	ISO_OPENARRAY	= ISO::OPENARRAY,
	ISO_REFERENCE	= ISO::REFERENCE,
	ISO_VIRTUAL		= ISO::VIRTUAL,
	ISO_USER		= ISO::USER,
	ISO_FUNCTION	= ISO::FUNCTION,
	ISO_TOTAL		= ISO::TOTAL;

static const ISO::Type::FLAGS
	TYPE_32BIT		= ISO::Type::TYPE_32BIT,
	TYPE_64BIT		= ISO::Type::TYPE_64BIT,
	TYPE_PACKED		= ISO::Type::TYPE_PACKED,
	TYPE_DODGY		= ISO::Type::TYPE_DODGY,
	TYPE_FIXED		= ISO::Type::TYPE_FIXED,
	TYPE_MASK		= ISO::Type::TYPE_MASK,
	TYPE_MASKEX		= ISO::Type::TYPE_MASKEX;

static const ISO::MATCH
	ISOMATCH_NOUSERRECURSE		= ISO::MATCH_NOUSERRECURSE,
	ISOMATCH_NOUSERRECURSE_RHS	= ISO::MATCH_NOUSERRECURSE_RHS,
	ISOMATCH_NOUSERRECURSE_BOTH = ISO::MATCH_NOUSERRECURSE_BOTH,
	ISOMATCH_MATCHNULLS			= ISO::MATCH_MATCHNULLS,
	ISOMATCH_MATCHNULL_RHS		= ISO::MATCH_MATCHNULL_RHS,
	ISOMATCH_IGNORE_SIZE		= ISO::MATCH_IGNORE_SIZE;

static const ISO::DUPF
	DUPF_DEEP			= ISO::DUPF_DEEP,
	DUPF_CHECKEXTERNALS = ISO::DUPF_CHECKEXTERNALS,
	DUPF_NOINITS		= ISO::DUPF_NOINITS,
	DUPF_EARLYOUT		= ISO::DUPF_EARLYOUT,
	DUPF_DUPSTRINGS		= ISO::DUPF_DUPSTRINGS;

template<typename T> auto ISO::getdef() { return ISO::getdef<T>(); }

using ISO::iso_bin_allocator;
using ISO::iso_nil;

using ISO::_MakePtr;
using ISO::_MakePtrExternal;

using ISO::GetUser;
using ISO::GetPtr;
using ISO::MakePtr;
using ISO::MakePtrExternal;
using ISO::MakePtrIndirect;
using ISO::ISO::MakeBrowser;
using ISO::MakePtrArray;
using ISO::LoadPtrArray;


}  // namespace iso

#endif  // ISO_COMPAT_H
