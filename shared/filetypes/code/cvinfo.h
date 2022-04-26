#pragma once
#include "base/strings.h"
#include "base/soft_float.h"
#include "cvconst.h"

#if 1
#include "base/maths.h"
#else
#include "OleAuto.h"
#undef small
#undef PURE
#endif

namespace CV {
using namespace iso;

#pragma pack ( push, 1 )

typedef uint16	type16_t;
typedef uint32	type_t;
typedef uint32	token_t;

typedef type_t ItemId;	// a stricter typeindex which may be referenced from symbol stream


struct octword	{ char b[32]; };
struct uoctword	{ char b[32]; };
typedef double	date;

#if 1
//96-bit unsigned integer with a power of 10 scaling factor specifying number of digits to the right of the decimal point (0-28)
class decimal {
	struct {
		uint16	reserved;
		uint8	scale, sign;
		uint32	mhi;
	};
	uint64	mlo;

	enum {DIGITS = 28};
	typedef soft_decimal_helpers<uint128>	helpers;

	decimal(uint8 sign, uint8 exp, uint128 mant) : reserved(0), scale(DIGITS - exp), sign(sign), mhi(hi(mant)), mlo(lo(mant))	{}

	uint128	get_mant()	const	{ return lo_hi(mlo, (uint64)mhi); }
	int		get_exp()	const	{ return DIGITS - scale; }

public:
	bool	get_sign()	const	{ return !!sign; }

	decimal()			{}
	decimal(double d) : scale(0), sign(d < 0 ? 0x80 : 0) {
		d		= abs(d);
		while (frac(d)) {
			d *= 10;
			++scale;
		}
		mhi	= uint64(d / iord::exp2(64).f());
		mlo	= uint64(d - mhi * iord::exp2(64).f());
	}
	operator double() const {
		return (mhi * iord::exp2(64).f() + mlo) / pow(10.0, uint32(scale));
	}
	friend int compare(const decimal &a, const decimal &b) {
		int	r = a.sign != b.sign ? 1 : helpers::compare_unsigned(a.get_mant(), a.get_exp(), b.get_mant(), b.get_exp());
		return a.sign ? -r : r;
	}
	friend decimal operator+(const decimal &a, const decimal &b) {
		int		e	= a.get_exp();
		auto	m	= a.get_mant();
		bool	s	= helpers::add(m, e, a.sign, b.get_mant(), b.get_exp(), b.sign);
		return decimal(s, e, m);
	}
	friend decimal operator-(const decimal &a, const decimal &b) {
		int		e	= a.get_exp();
		auto	m	= a.get_mant();
		bool	s	= helpers::add(m, e, a.sign, b.get_mant(), b.get_exp(), b.sign ^ 0x80);
		return decimal(s, e, m);
	}
	friend decimal operator*(const decimal &a, const decimal &b) {
		auto	m	= a.get_mant();
		int		d	= helpers::mul(m, b.get_mant(), DIGITS);
		return decimal(a.sign ^ b.sign, a.get_exp() + b.get_exp() + d - DIGITS, m);
	}
	friend decimal operator/(const decimal &a, const decimal &b) {
		auto	m	= a.get_mant();
		if (!m)
			return decimal(0, 0, 0);
		int		e	= helpers::strip_zeros<DIGITS>(m, a.get_exp() - b.get_exp() + helpers::div(m, b.get_mant(), DIGITS));
		return decimal(a.sign ^ b.sign, e, m);
	}
	friend decimal operator-(decimal a)  {
		a.sign ^= 0x80;
		return a;
	}
	friend decimal abs(decimal a) {
		a.sign = 0;
		return a;
	}
	friend decimal reciprocal(const decimal &a) {
		return decimal(1) / a;
	}
	friend decimal trunc(const decimal &a)	{
		auto	mant	= a.get_mant();
		int		guard	= helpers::div_pow10(mant, a.scale);
		return decimal(a.sign, 0, mant);
	}
	friend decimal floor(const decimal &a) {
		auto	mant	= a.get_mant();
		int		guard	= helpers::div_pow10(mant, a.scale);
		if (a.sign && guard)
			++mant;
		return decimal(a.sign, 0, mant);
	}
	friend decimal round(const decimal &a, int decimals) {
		if (decimals >= a.scale)
			return a;
		auto	mant	= a.get_mant();
		int		guard	= helpers::div_pow10(mant, a.scale - decimals);
		if (guard + (decimals & 1) >= 3)
			++mant;
		return decimal(a.sign, decimals, mant);
	}
	friend size_t to_string(char *s, const decimal &i) {
		return put_decimal(s, i.get_mant(), i.scale, !!i.sign);
	}
};

#else
struct decimal : DECIMAL, comparisons<decimal> {
	bool	get_sign()	const	{ return !!sign; }

	decimal()				{}
	decimal(double d)		{ VarDecFromR8(d, this); }
	operator double() const { double d; VarR8FromDec(this, &d); return d; }

	friend int compare(const decimal &a, const decimal &b)			{ return VarDecCmp(unconst(&a), unconst(&b)); }
	friend decimal operator+(const decimal &a, const decimal &b)	{ decimal r; VarDecAdd(unconst(&a), unconst(&b), &r); return r; }
	friend decimal operator-(const decimal &a, const decimal &b)	{ decimal r; VarDecSub(unconst(&a), unconst(&b), &r); return r; }
	friend decimal operator*(const decimal &a, const decimal &b)	{ decimal r; VarDecMul(unconst(&a), unconst(&b), &r); return r; }
	friend decimal operator/(const decimal &a, const decimal &b)	{ decimal r; VarDecDiv(unconst(&a), unconst(&b), &r); return r; }
	friend decimal operator-(const decimal &a)						{ decimal r; VarDecNeg(unconst(&a), &r); return r; }
	friend decimal abs(const decimal &a)							{ decimal r; VarDecAbs(unconst(&a), &r); return r; }
	friend decimal trunc(const decimal &a)							{ decimal r; VarDecInt(unconst(&a), &r); return r; }
	friend decimal floor(const decimal &a)							{ decimal r; VarDecFix(unconst(&a), &r); return r; }
	friend decimal round(const decimal &a, int decimals)			{ decimal r; VarDecRound(unconst(&a), decimals, &r); return r; }
	friend decimal reciprocal(const decimal &a)						{ return decimal(1) / a; }
	friend size_t to_string(char *s, const decimal &i)				{ return put_decimal(s, uint128(i.Lo64, i.Hi32), i.scale, !!i.sign); }
};
#endif

#define CV_SIGNATURE_C6			0L	// Actual signature is >64K
#define CV_SIGNATURE_C7			1L	// First explicit signature
#define CV_SIGNATURE_C11		2L	// C11 (vc5.x) 32-bit types
#define CV_SIGNATURE_C13		4L	// C13 (vc7.x) zero terminated names
#define CV_SIGNATURE_RESERVED	5L	// All signatures from 5 to 64K are reserved
#define CV_MAXOFFSET			0xffffffff

// Compress an integer (val) and store the result into pDataOut
// Return value is the number of bytes that the compressed data occupies
// Can only encode numbers up to 0x1FFFFFFF; returns -1 if val is too big to be compressed
inline uint32	CVCompressData(uint32 val, uint8 *pData) {
	if (val <= 0x7F) {
		pData[0] = uint8(val);
		return 1;
	}
	if (val <= 0x3FFF) {
		pData[0] = uint8((val >> 8) | 0x80);
		pData[1] = uint8(val & 0xff);
		return 2;
	}
	if (val <= 0x1FFFFFFF) {
		pData[0] = uint8((val >> 24) | 0xC0);
		pData[1] = uint8((val >> 16) & 0xff);
		pData[2] = uint8((val >> 8) & 0xff);
		pData[3] = uint8(val & 0xff);
		return 4;
	}
	return (uint32)-1;
}
// Uncompress the data in pData and store the result into pDataOut
// returns the uncompressed unsigned integer. pData is incremented to point to the next piece of uncompressed data
// returns -1 if what is passed in is incorrectly compressed data, such as (*pBytes & 0xE0) == 0xE0
inline uint32	CVUncompressData(const uint8 *&pData) {
	uint32 res = (uint32)(-1);
	if ((*pData & 0x80) == 0x00) {
		res = (uint32)(*pData++);
	} else if ((*pData & 0xC0) == 0x80) {
		res = (uint32)((*pData++ & 0x3f) << 8);
		res |= *pData++;
	} else if ((*pData & 0xE0) == 0xC0) {
		res = (*pData++ & 0x1f) << 24;
		res |= *pData++ << 16;
		res |= *pData++ << 8;
		res |= *pData++;
	}
	return res;
}
inline uint32	EncodeSignedInt32(int32 input)	{ return input >= 0 ? input << 1 : ((-input) << 1) | 1; }
inline int32	DecodeSignedInt32(uint32 input)	{ return input & 1 ? -int(input >> 1) : int(input >> 1); }

//-----------------------------------------------------------------------------
//	Types
//-----------------------------------------------------------------------------

// Type indices less than 0x1000 are primitive types, defined as:
//		uint16
//			subtype:4,
//			type:4,
//			mode:3, :1	// pointer mode
// Type indices above 0x1000 are used to describe more complex features such as functions, arrays and structures

#define CV_MMASK	0x700	// mode mask
#define CV_TMASK	0x0f0	// type mask
#define CV_SMASK	0x00f	// subtype mask
#define CV_MSHIFT	8		// primitive mode right shift count
#define CV_TSHIFT	4		// primitive type right shift count
#define CV_SSHIFT	0		// primitive subtype right shift count

// macros to extract primitive mode, type and size
#define CV_MODE(typ)	(((typ) & CV_MMASK) >> CV_MSHIFT)
#define CV_TYPE(typ)	(((typ) & CV_TMASK) >> CV_TSHIFT)
#define CV_SUBT(typ)	(((typ) & CV_SMASK) >> CV_SSHIFT)

//	pointer mode enumeration values
enum prmode_e {
	TM_DIRECT	= 0,		// mode is not a pointer
	TM_NPTR		= 1,		// mode is a near pointer
	TM_FPTR		= 2,		// mode is a far pointer
	TM_HPTR		= 3,		// mode is a huge pointer
	TM_NPTR32	= 4,		// mode is a 32 bit near pointer
	TM_FPTR32	= 5,		// mode is a 32 bit far pointer
	TM_NPTR64	= 6,		// mode is a 64 bit near pointer
	TM_NPTR128	= 7,		// mode is a 128 bit near pointer
};

//	type enumeration values
enum type_e {
	SPECIAL		= 0x00,		// special type size values
	SIGNED		= 0x01,		// signed integral size values
	UNSIGNED	= 0x02,		// unsigned integral size values
	BOOLEAN		= 0x03,		// Boolean size values
	REAL		= 0x04,		// real number size values
	COMPLEX		= 0x05,		// complex number size values
	SPECIAL2	= 0x06,		// second set of special types
	INT			= 0x07,		// integral (int) values
	CVRESERVED	= 0x0f,
};

//	subtype enumeration values for SPECIAL
enum special_e {
	SP_NOTYPE	= 0x00,
	SP_ABS		= 0x01,
	SP_SEGMENT	= 0x02,
	SP_VOID		= 0x03,
	SP_CURRENCY	= 0x04,
	SP_NBASICSTR = 0x05,
	SP_FBASICSTR = 0x06,
	SP_NOTTRANS	= 0x07,
	SP_HRESULT	= 0x08,
};

//	subtype enumeration values for SPECIAL2
enum special2_e {
	S2_BIT		= 0x00,
	S2_PASCHAR	= 0x01,	// Pascal CHAR
	S2_BOOL32FF	= 0x02,	// 32-bit BOOL where true is 0xffffffff
};

//	subtype enumeration values for SIGNED, UNSIGNED and BOOLEAN
enum integral_e {
	IN_1BYTE	= 0x00,
	IN_2BYTE	= 0x01,
	IN_4BYTE	= 0x02,
	IN_8BYTE	= 0x03,
	IN_16BYTE	= 0x04
};

//	subtype enumeration values for REAL and COMPLEX
enum real_e {
	RC_REAL32	= 0x00,
	RC_REAL64	= 0x01,
	RC_REAL80	= 0x02,
	RC_REAL128	= 0x03,
	RC_REAL48	= 0x04,
	RC_REAL32PP	= 0x05,	// 32-bit partial precision real
	RC_REAL16	= 0x06,
};

//	subtype enumeration values for INT (really int)
enum int_e {
	RI_CHAR		= 0x00,
	RI_INT1		= 0x00,
	RI_WCHAR	= 0x01,
	RI_UINT1	= 0x01,
	RI_INT2		= 0x02,
	RI_UINT2	= 0x03,
	RI_INT4		= 0x04,
	RI_UINT4	= 0x05,
	RI_INT8		= 0x06,
	RI_UINT8	= 0x07,
	RI_INT16	= 0x08,
	RI_UINT16	= 0x09,
	RI_CHAR16	= 0x0a,	// char16_t
	RI_CHAR32	= 0x0b,	// char32_t
};

enum LEAF_ENUM_e : uint16 {
	// 1st range: leaf indices starting records but referenced from symbol records
	LF_MODIFIER_16t		= 0x0001,
	LF_POINTER_16t		= 0x0002,
	LF_ARRAY_16t		= 0x0003,
	LF_CLASS_16t		= 0x0004,
	LF_STRUCTURE_16t	= 0x0005,
	LF_UNION_16t		= 0x0006,
	LF_ENUM_16t			= 0x0007,
	LF_PROCEDURE_16t	= 0x0008,
	LF_MFUNCTION_16t	= 0x0009,
	LF_VTSHAPE			= 0x000a,
	LF_COBOL0_16t		= 0x000b,
	LF_COBOL1			= 0x000c,
	LF_BARRAY_16t		= 0x000d,
	LF_LABEL			= 0x000e,
	LF_NULL				= 0x000f,
	LF_NOTTRAN			= 0x0010,
	LF_DIMARRAY_16t		= 0x0011,
	LF_VFTPATH_16t		= 0x0012,
	LF_PRECOMP_16t		= 0x0013,	// not referenced from symbol
	LF_ENDPRECOMP		= 0x0014,	// not referenced from symbol
	LF_OEM_16t			= 0x0015,	// oem definable type string
	LF_TYPESERVER_ST	= 0x0016,	// not referenced from symbol

	// 2nd range: leaf indices starting records but referenced only from type records
	LF_SKIP_16t			= 0x0200,
	LF_ARGLIST_16t		= 0x0201,
	LF_DEFARG_16t		= 0x0202,
	LF_LIST				= 0x0203,
	LF_FIELDLIST_16t	= 0x0204,
	LF_DERIVED_16t		= 0x0205,
	LF_BITFIELD_16t		= 0x0206,
	LF_METHODLIST_16t	= 0x0207,
	LF_DIMCONU_16t		= 0x0208,
	LF_DIMCONLU_16t		= 0x0209,
	LF_DIMVARU_16t		= 0x020a,
	LF_DIMVARLU_16t		= 0x020b,
	LF_REFSYM			= 0x020c,

	// 3rd range: used to build up complex lists such as the field list of a class type record. No type record can begin with one of the leaf indices
	LF_BCLASS_16t		= 0x0400,
	LF_VBCLASS_16t		= 0x0401,
	LF_IVBCLASS_16t		= 0x0402,
	LF_ENUMERATE_ST		= 0x0403,
	LF_FRIENDFCN_16t	= 0x0404,
	LF_INDEX_16t		= 0x0405,
	LF_MEMBER_16t		= 0x0406,
	LF_STMEMBER_16t		= 0x0407,
	LF_METHOD_16t		= 0x0408,
	LF_NESTTYPE_16t		= 0x0409,
	LF_VFUNCTAB_16t		= 0x040a,
	LF_FRIENDCLS_16t	= 0x040b,
	LF_ONEMETHOD_16t	= 0x040c,
	LF_VFUNCOFF_16t		= 0x040d,

	// 32-bit type index versions of leaves, all have the 0x1000 bit set
	LF_TI16_MAX			= 0x1000,
	LF_MODIFIER			= 0x1001,
	LF_POINTER			= 0x1002,
	LF_ARRAY_ST			= 0x1003,
	LF_CLASS_ST			= 0x1004,
	LF_STRUCTURE_ST		= 0x1005,
	LF_UNION_ST			= 0x1006,
	LF_ENUM_ST			= 0x1007,
	LF_PROCEDURE		= 0x1008,
	LF_MFUNCTION		= 0x1009,
	LF_COBOL0			= 0x100a,
	LF_BARRAY			= 0x100b,
	LF_DIMARRAY_ST		= 0x100c,
	LF_VFTPATH			= 0x100d,
	LF_PRECOMP_ST		= 0x100e,	// not referenced from symbol
	LF_OEM				= 0x100f,	// oem definable type string
	LF_ALIAS_ST			= 0x1010,	// alias (typedef) type
	LF_OEM2				= 0x1011,	// oem definable type string

	LF_SKIP				= 0x1200,
	LF_ARGLIST			= 0x1201,
	LF_DEFARG_ST		= 0x1202,
	LF_FIELDLIST		= 0x1203,
	LF_DERIVED			= 0x1204,
	LF_BITFIELD			= 0x1205,
	LF_METHODLIST		= 0x1206,
	LF_DIMCONU			= 0x1207,
	LF_DIMCONLU			= 0x1208,
	LF_DIMVARU			= 0x1209,
	LF_DIMVARLU			= 0x120a,

	LF_BCLASS			= 0x1400,
	LF_VBCLASS			= 0x1401,
	LF_IVBCLASS			= 0x1402,
	LF_FRIENDFCN_ST		= 0x1403,
	LF_INDEX			= 0x1404,
	LF_MEMBER_ST		= 0x1405,
	LF_STMEMBER_ST		= 0x1406,
	LF_METHOD_ST		= 0x1407,
	LF_NESTTYPE_ST		= 0x1408,
	LF_VFUNCTAB			= 0x1409,
	LF_FRIENDCLS		= 0x140a,
	LF_ONEMETHOD_ST		= 0x140b,
	LF_VFUNCOFF			= 0x140c,
	LF_NESTTYPEEX_ST	= 0x140d,
	LF_MEMBERMODIFY_ST	= 0x140e,
	LF_MANAGED_ST		= 0x140f,

	// Types w/ SZ names
	LF_ST_MAX			= 0x1500,
	LF_TYPESERVER		= 0x1501,	// not referenced from symbol
	LF_ENUMERATE		= 0x1502,
	LF_ARRAY			= 0x1503,
	LF_CLASS			= 0x1504,
	LF_STRUCTURE		= 0x1505,
	LF_UNION			= 0x1506,
	LF_ENUM				= 0x1507,
	LF_DIMARRAY			= 0x1508,
	LF_PRECOMP			= 0x1509,	// not referenced from symbol
	LF_ALIAS			= 0x150a,	// alias (typedef) type
	LF_DEFARG			= 0x150b,
	LF_FRIENDFCN		= 0x150c,
	LF_MEMBER			= 0x150d,
	LF_STMEMBER			= 0x150e,
	LF_METHOD			= 0x150f,
	LF_NESTTYPE			= 0x1510,
	LF_ONEMETHOD		= 0x1511,
	LF_NESTTYPEEX		= 0x1512,
	LF_MEMBERMODIFY		= 0x1513,
	LF_MANAGED			= 0x1514,
	LF_TYPESERVER2		= 0x1515,
	LF_STRIDED_ARRAY	= 0x1516,	// same as LF_ARRAY, but with stride between adjacent elements
	LF_HLSL				= 0x1517,
	LF_MODIFIER_EX		= 0x1518,
	LF_INTERFACE		= 0x1519,
	LF_BINTERFACE		= 0x151a,
	LF_VECTOR			= 0x151b,
	LF_MATRIX			= 0x151c,
	LF_VFTABLE			= 0x151d,	// a virtual function table

	LF_TYPE_LAST,					// one greater than the last type record

	LF_FUNC_ID			= 0x1601,	// global func ID
	LF_MFUNC_ID			= 0x1602,	// member func ID
	LF_BUILDINFO		= 0x1603,	// build info: tool, version, command line, src/pdb file
	LF_SUBSTR_LIST		= 0x1604,	// similar to LF_ARGLIST, for list of sub strings
	LF_STRING_ID		= 0x1605,	// string ID
	LF_UDT_SRC_LINE		= 0x1606,	// source and line on where an UDT is defined; only generated by compiler
	LF_UDT_MOD_SRC_LINE = 0x1607,	// module, source and line on where an UDT is defined

	LF_ID_LAST,						// one greater than the last ID record

//	Used to represent numeric data in a symbol or type record; these leaf indices are greater than 0x8000
//	When expecting a numeric field, if the next two bytes are less than 0x8000, then this is the numeric value; otherwise the data follows the leaf index in a format specified by the leaf index

	LF_NUMERIC			= 0x8000,
	LF_CHAR				= 0x8000,
	LF_SHORT			= 0x8001,
	LF_USHORT			= 0x8002,
	LF_LONG				= 0x8003,
	LF_ULONG			= 0x8004,
	LF_REAL32			= 0x8005,
	LF_REAL64			= 0x8006,
	LF_REAL80			= 0x8007,
	LF_REAL128			= 0x8008,
	LF_QUADWORD			= 0x8009,
	LF_UQUADWORD		= 0x800a,
	LF_REAL48			= 0x800b,
	LF_COMPLEX32		= 0x800c,
	LF_COMPLEX64		= 0x800d,
	LF_COMPLEX80		= 0x800e,
	LF_COMPLEX128		= 0x800f,
	LF_VARSTRING		= 0x8010,
	LF_OCTWORD			= 0x8017,
	LF_UOCTWORD			= 0x8018,
	LF_DECIMAL			= 0x8019,
	LF_DATE				= 0x801a,
	LF_UTF8STRING		= 0x801b,
	LF_REAL16			= 0x801c,

//	used to force alignment of subfields within a complex type record
	LF_PAD0				= 0xf0,
	LF_PAD1				= 0xf1,
	LF_PAD2				= 0xf2,
	LF_PAD3				= 0xf3,
	LF_PAD4				= 0xf4,
	LF_PAD5				= 0xf5,
	LF_PAD6				= 0xf6,
	LF_PAD7				= 0xf7,
	LF_PAD8				= 0xf8,
	LF_PAD9				= 0xf9,
	LF_PAD10			= 0xfa,
	LF_PAD11			= 0xfb,
	LF_PAD12			= 0xfc,
	LF_PAD13			= 0xfd,
	LF_PAD14			= 0xfe,
	LF_PAD15			= 0xff,
};

//	class/struct/union/enum properties
struct prop_t {
	//	enumeration for HFA kinds
	enum HFA {
		HFA_none		= 0,
		HFA_float		= 1,
		HFA_double		= 2,
		HFA_other		= 3
	};
	//	enumeration for MoCOM UDT kinds
	enum MOCOM {
		MOCOM_none		= 0,
		MOCOM_ref		= 1,
		MOCOM_value		= 2,
		MOCOM_interface	= 3
	};
	uint16		packed			: 1;	// true if structure is packed
	uint16		ctor			: 1;	// true if constructors or destructors present
	uint16		ovlops			: 1;	// true if overloaded operators present
	uint16		isnested		: 1;	// true if this is a nested class
	uint16		cnested			: 1;	// true if this class contains nested types
	uint16		opassign		: 1;	// true if overloaded assignment (=)
	uint16		opcast			: 1;	// true if casting methods
	uint16		fwdref			: 1;	// true if forward reference (incomplete defn)
	uint16		scoped			: 1;	// scoped definition
	uint16		hasuniquename	: 1;	// true if there is a decorated name following the regular name
	uint16		sealed			: 1;	// true if class cannot be used as a base class
	uint16		hfa				: 2;	// HFA
	uint16		intrinsic		: 1;	// true if class is an intrinsic type (e.g. __m128d)
	uint16		mocom			: 2;	// MOCOM
};

//	class field attribute
struct fldattr_t {
//	enumeration for method properties
	enum methodprop {
		Vanilla		= 0x00,
		Virtual		= 0x01,
		Static		= 0x02,
		Friend		= 0x03,
		Intro		= 0x04,
		Purevirt	= 0x05,
		Pureintro	= 0x06
	};
	uint16		access			: 2;	// access protection access_t
	uint16		mprop			: 3;	// method properties methodprop
	uint16		pseudo			: 1;	// compiler generated fcn and does not exist
	uint16		noinherit		: 1;	// true if class cannot be inherited
	uint16		noconstruct		: 1;	// true if class cannot be constructed
	uint16		compgenx		: 1;	// compiler generated fcn and does exist
	uint16		sealed			: 1;	// true if method cannot be overridden
	uint16		unused			: 6;	// unused

	int	extra() const {
		return int(mprop == Intro || mprop == Pureintro);
	}
};

//	function flags
struct funcattr_t {
	uint8		cxxreturnudt	: 1;	// true if C++ style ReturnUDT
	uint8		ctor			: 1;	// true if func is an instance constructor
	uint8		ctorvbase		: 1;	// true if func is an instance constructor of a class with virtual bases
	uint8		unused			: 5;	// unused
};

//	base type record
struct Leaf {
	LEAF_ENUM_e		leaf;	// LF_...
	template<typename T> T*			as()			{ return static_cast<T*>(this); }
	template<typename T> const T*	as()	const	{ return static_cast<const T*>(this); }
};

struct TYPTYPE_len {
	uint16		len;
};
struct TYPTYPE : TYPTYPE_len, Leaf {
	uint16				size()	const	{ return len + sizeof(uint16); }
	const_memory_block	data()	const	{ return const_memory_block(this + 1, len); }
	TYPTYPE				*next()			{ return (TYPTYPE*)((uint8*)this + size()); }
	const TYPTYPE		*next()	const	{ return (const TYPTYPE*)((const uint8*)this + size()); }
};

//	variable length numeric field
struct VarString : Leaf { // LF_VARSTRING
	uint16			len;		// length of value in bytes
	uint8			val[];		// value
	count_string value() const { return str((const char*)val, len); }
};

template<typename T> struct ValueT : Leaf {
	T	val;
};

struct Value : Leaf {
	enum Kind {
		Unknown,
		Int,
		Float,
		Complex,
		String,
		Special,
	};
	uint32	size() const {
		static const uint8 skip[] = {
			sizeof(char),			//LF_CHAR
			sizeof(short),			//LF_SHORT
			sizeof(short),			//LF_USHORT
			sizeof(long),			//LF_LONG
			sizeof(ulong),			//LF_ULONG
			sizeof(float),			//LF_REAL32
			sizeof(double),			//LF_REAL64
			sizeof(float80),		//LF_REAL80
			sizeof(uint128),		//LF_REAL128
			sizeof(uint64),			//LF_QUADWORD
			sizeof(uint64),			//LF_UQUADWORD
			sizeof(float48),		//LF_REAL48
			sizeof(float)	* 2,	//LF_COMPLEX32
			sizeof(double)	* 2,	//LF_COMPLEX64
			sizeof(float80) * 2,	//LF_COMPLEX80
			sizeof(uint128) * 2,	//LF_COMPLEX128
			0,						//LF_VARSTRING
			0,						//11
			0,						//12
			0,						//13
			0,						//14
			0,						//15
			0,						//16
			sizeof(uint128),		//LF_OCTWORD
			sizeof(uint128),		//LF_UOCTWORD
			sizeof(decimal),		//LF_DECIMAL
			sizeof(date),			//LF_DATE
			0,						//LF_UTF8STRING
			sizeof(uint16),			//LF_REAL16
		};
		uint8	n
			= leaf == LF_UTF8STRING || leaf == LF_VARSTRING	? ((const VarString*)this)->len + 2
			: leaf - 0x8000 < num_elements(skip)			? skip[leaf - 0x8000]
			: 0;
		return n + 2;
	}
	Kind	kind() const {
		if (!(leaf & 0x8000))
			return Int;

		switch (leaf) {
			case LF_CHAR:		case LF_SHORT:		case LF_USHORT:		case LF_LONG:	case LF_ULONG:	case LF_QUADWORD:	case LF_UQUADWORD:
				return Int;
			case LF_REAL16:		case LF_REAL32:		case LF_REAL64:		case LF_REAL80:	case LF_REAL48:	case LF_REAL128:	case LF_DATE:	case LF_DECIMAL:
				return Float;
			case LF_COMPLEX32:	case LF_COMPLEX64:	case LF_COMPLEX80:	case LF_COMPLEX128:
				return Complex;
			case LF_VARSTRING:	case LF_UTF8STRING:
				return String;
			case LF_OCTWORD:	case LF_UOCTWORD:
				return Special;
			default:
				return Unknown;
		}
	}
	bool	operator!() const {
		return leaf == 0;
	}
	operator int64() const {
		uint8	n = 0;
		if (!(leaf & 0x8000))
			return leaf;

		switch (leaf) {
			case LF_CHAR:		return as<ValueT<char	>>()->val;
			case LF_SHORT:		return as<ValueT<int16	>>()->val;
			case LF_USHORT:		return as<ValueT<uint16	>>()->val;
			case LF_LONG:		return as<ValueT<long	>>()->val;
			case LF_ULONG:		return as<ValueT<ulong	>>()->val;
			case LF_QUADWORD:	return as<ValueT<int64	>>()->val;
			case LF_UQUADWORD:	return as<ValueT<uint64	>>()->val;
			default: ISO_ASSERT(0); return -1;
		}
	}
	operator float64() const {
		uint8	n = 0;
		if (!(leaf & 0x8000))
			return leaf;

		switch (leaf) {
			case LF_CHAR:		return as<ValueT<char	>>()->val;
			case LF_SHORT:		return as<ValueT<int16	>>()->val;
			case LF_USHORT:		return as<ValueT<uint16	>>()->val;
			case LF_LONG:		return as<ValueT<long	>>()->val;
			case LF_ULONG:		return as<ValueT<ulong	>>()->val;
			case LF_QUADWORD:	return as<ValueT<int64	>>()->val;
			case LF_UQUADWORD:	return as<ValueT<uint64	>>()->val;
			case LF_REAL32:		return as<ValueT<float32>>()->val;
			case LF_REAL64:		return as<ValueT<float64>>()->val;
			case LF_REAL80:		return (float64)as<ValueT<float80	>>()->val;
			case LF_REAL128:	return (float64)as<ValueT<float128	>>()->val;
			case LF_REAL48:		return (float64)as<ValueT<float48	>>()->val;
			case LF_REAL16:		return as<ValueT<float16>>()->val;
			case LF_DATE:		return as<ValueT<date	>>()->val;
			case LF_DECIMAL:	return as<ValueT<decimal>>()->val;
			default: ISO_ASSERT(0); return -1;
		}
	}
	const char	*after() const { return (const char*)this + size(); }
};

inline uint32 cbNumField(const uint8 *pb) {
	return ((const Value*)pb)->size();
}

enum PMEMBER {
	PDM16_NONVIRT	= 0x00, // 16:16 data no virtual fcn or base
	PDM16_VFCN		= 0x01, // 16:16 data with virtual functions
	PDM16_VBASE		= 0x02, // 16:16 data with virtual bases
	PDM32_NVVFCN	= 0x03, // 16:32 data w/wo virtual functions
	PDM32_VBASE		= 0x04, // 16:32 data with virtual bases
	PMF16_NEARNVSA	= 0x05, // 16:16 near method nonvirtual single address point
	PMF16_NEARNVMA	= 0x06, // 16:16 near method nonvirtual multiple address points
	PMF16_NEARVBASE	= 0x07, // 16:16 near method virtual bases
	PMF16_FARNVSA	= 0x08, // 16:16 far method nonvirtual single address point
	PMF16_FARNVMA	= 0x09, // 16:16 far method nonvirtual multiple address points
	PMF16_FARVBASE	= 0x0a, // 16:16 far method virtual bases
	PMF32_NVSA		= 0x0b, // 16:32 method nonvirtual single address point
	PMF32_NVMA		= 0x0c, // 16:32 method nonvirtual multiple address point
	PMF32_VBASE		= 0x0d	// 16:32 method virtual bases
};

struct segmented16 {
	uint16	off;
	uint16		seg;

	friend int	compare(const segmented16 &a, const segmented16 &b) {
		return a.seg != b.seg ? int(a.seg) - int(b.seg) : int(a.off) - int(b.off);
	}
};

// ensure packed
struct segmented32 {
	uint32	off;
	uint16	seg;

	segmented32	operator+(uint32 x) const { return {off + x, seg}; }

	friend int	compare(const segmented32 &a, const segmented32 &b) {
		return a.seg != b.seg ? int(a.seg) - int(b.seg) : int(a.off) - int(b.off);
	}
};

//	memory representation of pointer to member
//	These representations are indexed by the enumeration above in the LF_POINTER record representation of a 16:16 pointer to data for a class with no virtual functions or virtual bases
struct PDMR16_NONVIRT {
	int16		mdisp;	// displacement to data (NULL = -1)
};
//	representation of a 16:16 pointer to data for a class with virtual functions
struct PMDR16_VFCN {
	int16		mdisp;	// displacement to data ( NULL = 0)
};
//	representation of a 16:16 pointer to data for a class with virtual bases
struct PDMR16_VBASE {
	int16		mdisp;	// displacement to data
	int16		pdisp;	// this pointer displacement to vbptr
	int16		vdisp;	// displacement within vbase table NULL = (,,0xffff)
};
//	representation of a 32 bit pointer to data for a class with or without virtual functions and no virtual bases
struct PDMR32_NVVFCN {
	int32		mdisp;	// displacement to data (NULL = 0x80000000)
};
//	representation of a 32 bit pointer to data for a class with virtual bases
struct PDMR32_VBASE {
	int32		mdisp;	// displacement to data
	int32		pdisp;	// this pointer displacement
	int32		vdisp;	// vbase table displacement NULL = (,,0xffffffff)
};
//	representation of a 16:16 pointer to near member function for a class with no virtual functions or bases and a single address point
struct PMFR16_NEARNVSA {
	uint16	off;	// near address of function (NULL = 0)
};
//	representation of a 16 bit pointer to member functions of a class with no virtual bases and multiple address points
struct PMFR16_NEARNVMA {
	uint16	off;	// offset of function (NULL = 0,x)
	int16		disp;
};
//	representation of a 16 bit pointer to member function of a class with virtual bases
struct PMFR16_NEARVBASE {
	uint16	off;	// offset of function (NULL = 0,x,x,x)
	int16		mdisp;	// displacement to data
	int16		pdisp;	// this pointer displacement
	int16		vdisp;	// vbase table displacement
};
//	representation of a 16:16 pointer to far member function for a class with no virtual bases and a single address point
struct PMFR16_FARNVSA {
	segmented16	addr;	// addr of function (NULL = 0:0)
};
//	representation of a 16:16 far pointer to member functions of a class with no virtual bases and multiple address points
struct PMFR16_FARNVMA {
	segmented16	addr;	// addr of function (NULL = 0:0)
	int16		disp;
};
//	representation of a 16:16 far pointer to member function of a class with virtual bases
struct PMFR16_FARVBASE {
	segmented16	addr;	// offset of function (NULL = 0:0,x,x,x)
	int16		mdisp;	// displacement to data
	int16		pdisp;	// this pointer displacement
	int16		vdisp;	// vbase table displacement
};
//	representation of a 32 bit pointer to member function for a class with no virtual bases and a single address point
struct PMFR32_NVSA {
	uint32	off;	// near address of function (NULL = 0L)
};
//	representation of a 32 bit pointer to member function for a class with no virtual bases and multiple address points
struct PMFR32_NVMA {
	uint32	off;	// near address of function (NULL = 0L,x)
	int32		disp;
};
//	representation of a 32 bit pointer to member function for a class with virtual bases
struct PMFR32_VBASE {
	uint32	off;	// near address of function (NULL = 0L,x,x,x)
	int32		mdisp;	// displacement to data
	int32		pdisp;	// this pointer displacement
	int32		vdisp;	// vbase table displacement
};

//	Notes on alignment
//	Alignment of the fields in most of the type records is done on the basis of the TYPTYPE record base
//	That is why in most of the * records the type_t (32-bit types) is located on what appears to be a offset mod 4 == 2 boundary
//	The exception to this rule are those records that are in a list (FieldList, MethodList), which are aligned to their own bases since they don't have the length field

struct modifier_t {
	uint16		MOD_const:1, MOD_volatile:1, MOD_unaligned:1, MOD_unused:13;
};

struct Modifier_16t : Leaf { // LF_MODIFIER_16t
	modifier_t	attr;	// modifier attribute modifier_t
	type16_t		type;	// modified type
};
struct Modifier : Leaf { // LF_MODIFIER
	type_t		type;	// modified type
	modifier_t	attr;	// modifier attribute modifier_t
};

//	POINTER

//	type for pointer records
enum ptrtype_e {
	PTR_NEAR			= 0x00, // 16 bit pointer
	PTR_FAR				= 0x01, // 16:16 far pointer
	PTR_HUGE			= 0x02, // 16:16 huge pointer
	PTR_BASE_SEG		= 0x03, // based on segment
	PTR_BASE_VAL		= 0x04, // based on value of base
	PTR_BASE_SEGVAL		= 0x05, // based on segment value of base
	PTR_BASE_ADDR		= 0x06, // based on address of base
	PTR_BASE_SEGADDR	= 0x07, // based on segment address of base
	PTR_BASE_TYPE		= 0x08, // based on type
	PTR_BASE_SELF		= 0x09, // based on self
	PTR_NEAR32			= 0x0a, // 32 bit pointer
	PTR_FAR32			= 0x0b, // 16:32 pointer
	PTR_64				= 0x0c, // 64 bit pointer
	PTR_UNUSEDPTR		= 0x0d	// first unused pointer type
};

//	modes for pointers
enum ptrmode_e {
	PTR_MODE_PTR		= 0x00, // "normal" pointer
	PTR_MODE_REF		= 0x01, // "old" reference
	PTR_MODE_LVREF		= 0x01, // l-value reference
	PTR_MODE_PMEM		= 0x02, // pointer to data member
	PTR_MODE_PMFUNC		= 0x03, // pointer to member function
	PTR_MODE_RVREF		= 0x04, // r-value reference
	PTR_MODE_RESERVED	= 0x05	// first unused pointer mode
};

//	pointer-to-member types
enum pmtype_e {
	PMTYPE_Undef		= 0x00, // not specified (pre VC8)
	PMTYPE_D_Single		= 0x01, // member data, single inheritance
	PMTYPE_D_Multiple	= 0x02, // member data, multiple inheritance
	PMTYPE_D_Virtual	= 0x03, // member data, virtual inheritance
	PMTYPE_D_General	= 0x04, // member data, most general
	PMTYPE_F_Single		= 0x05, // member function, single inheritance
	PMTYPE_F_Multiple	= 0x06, // member function, multiple inheritance
	PMTYPE_F_Virtual	= 0x07, // member function, virtual inheritance
	PMTYPE_F_General	= 0x08, // member function, most general
};

struct Pointer_16t : Leaf { // LF_POINTER_16t
	struct PointerAttr_16t {
		uint8	ptrtype		: 5;	// ordinal specifying pointer type (ptrtype_e)
		uint8	ptrmode		: 3;	// ordinal specifying pointer mode (ptrmode_e)
		uint8	isflat32	: 1;	// true if 0:32 pointer
		uint8	isvolatile	: 1;	// TRUE if volatile pointer
		uint8	isconst		: 1;	// TRUE if const pointer
		uint8	isunaligned : 1;	// TRUE if unaligned pointer
		uint8	unused		: 4;
	} attr;
	type16_t	utype;				// type index of the underlying type
	union {
		struct {
			type16_t	pmclass;	// index of containing class for pointer to member
			uint16		pmenum;		// enumeration specifying pm format (pmtype_e)
		} pm;
		uint16		bseg;			// base segment if PTR_BASE_SEG
		uint8		Sym[1];			// copy of base symbol record (including length)
		struct {
			type16_t		index;	// type index if PTR_BASE_TYPE
			embedded_string	name;	// name of base type
		} btype;
	} pbase;
};

struct Pointer : Leaf { // LF_POINTER
	type_t	utype;				// type index of the underlying type
	struct PointerAttr {
		uint32		ptrtype		: 5;	// ordinal specifying pointer type (ptrtype_e)
		uint32		ptrmode		: 3;	// ordinal specifying pointer mode (ptrmode_e)
		uint32		isflat32	: 1;	// true if 0:32 pointer
		uint32		isvolatile	: 1;	// TRUE if volatile pointer
		uint32		isconst		: 1;	// TRUE if const pointer
		uint32		isunaligned	: 1;	// TRUE if unaligned pointer
		uint32		isrestrict	: 1;	// TRUE if restricted pointer (allow agressive opts)
		uint32		size		: 6;	// size of pointer (in bytes)
		uint32		ismocom		: 1;	// TRUE if it is a MoCOM pointer (^ or %)
		uint32		islref		: 1;	// TRUE if it is this pointer of member function with & ref-qualifier
		uint32		isrref		: 1;	// TRUE if it is this pointer of member function with && ref-qualifier
		uint32		unused		: 10;	// pad out to 32-bits for following cv_typ_t's
	} attr;
	union {
		struct {
			type_t		pmclass;		// index of containing class for pointer to member
			uint16		pmenum;			// enumeration specifying pm format (pmtype_e)
		} pm;
		uint16		bseg;				// base segment if PTR_BASE_SEG
		uint8		Sym[1];				// copy of base symbol record (including length)
		struct {
			type_t			index;		// type index if PTR_BASE_TYPE
			embedded_string	name;		// name of base type
		} btype;
	} pbase;
};

//	ARRAY
struct Array_16t : Leaf { // LF_ARRAY_16t
	type16_t	elemtype;	// type index of element type
	type16_t	idxtype;	// type index of indexing type
	Value		size;		// size in bytes
	const char*	name()	const	{ return size.after(); }
};
struct Array : Leaf { // LF_ARRAY
	type_t		elemtype;	// type index of element type
	type_t		idxtype;	// type index of indexing type
	Value		size;		// size in bytes
	const char*	name()	const	{ return size.after(); }
};
struct StridedArray : Leaf { // LF_STRIDED_ARRAY
	type_t		elemtype;	// type index of element type
	type_t		idxtype;	// type index of indexing type
	uint32		stride;
	Value		size;		// size in bytes
	const char*	name()	const	{ return size.after(); }
};

//	VECTOR
struct Vector : Leaf { // LF_VECTOR
	type_t		elemtype;	// type index of element type
	uint32		count;		// number of elements in the vector
	Value		size;		// size in bytes
	const char*	name()	const	{ return size.after(); }
};

//	MATRIX
struct Matrix : Leaf { // LF_MATRIX
	type_t		elemtype;	// type index of element type
	uint32		rows;		// number of rows
	uint32		cols;		// number of columns
	uint32		majorStride;
	struct {
		uint8	row_major: 1;	// true if matrix has row-major layout (column-major is default)
		uint8	unused:7;
	}			matattr;	// attributes
	Value		size;		// size in bytes
	const char*	name()	const	{ return size.after(); }
};

//	CLASS, STRUCTURE
struct Class_16t : Leaf { // LF_CLASS_16t, LF_STRUCT_16t
	uint16		count;		// count of number of elements in class
	type16_t	field;		// type index of LF_FIELD descriptor list
	prop_t		property;	// property attribute field (prop_t)
	type16_t	derived;	// type index of derived from list if not zero
	type16_t	vshape;		// type index of vshape table for this class
	Value		size;		// size of structure in bytes
	const char*	name()	const	{ return size.after(); }
};
typedef Class_16t Structure_16t;
struct Class : Leaf { // LF_CLASS, LF_STRUCT, LF_INTERFACE
	uint16		count;		// count of number of elements in class
	prop_t		property;	// property attribute field (prop_t)
	type_t		field;		// type index of LF_FIELD descriptor list
	type_t		derived;	// type index of derived from list if not zero
	type_t		vshape;		// type index of vshape table for this class
	Value		size;		// size of structure in bytes
	const char*	name()	const	{ return size.after(); }
};
typedef Class Structure;
typedef Class Interface;

//	UNION
struct Union_16t : Leaf { // LF_UNION_16t
	uint16		count;		// count of number of elements in class
	type16_t	field;		// type index of LF_FIELD descriptor list
	prop_t		property;	// property attribute field
	Value		size;		// size of structure in bytes
	const char*	name()	const	{ return size.after(); }
};
struct Union : Leaf { // LF_UNION
	uint16		count;		// count of number of elements in class
	prop_t		property;	// property attribute field
	type_t		field;		// type index of LF_FIELD descriptor list
	Value		size;		// size of structure in bytes
	const char*	name()	const	{ return size.after(); }
};

//	ALIAS
struct Alias : Leaf { // LF_ALIAS
	type_t			utype;		// underlying type
	embedded_string	name;		// alias name
};

struct FuncId : Leaf { // LF_FUNC_ID
	ItemId			scopeId;	// parent scope of the ID, 0 if global
	type_t			type;		// function type
	embedded_string	name;
};
struct MFuncId : Leaf { // LF_MFUNC_ID
	type_t			parentType;	// type index of parent
	type_t			type;		// function type
	embedded_string	name;
};
struct StringId : Leaf { // LF_STRING_ID
	ItemId			id;			// ID to list of sub string IDs
	embedded_string	name;
};
struct UdtSrcLine : Leaf { // LF_UDT_SRC_LINE
	type_t			type;		// UDT's type index
	ItemId			src;		// index to LF_STRING_ID record where source file name is saved
	uint32			line;		// line number
};
struct UdtModSrcLine : Leaf { // LF_UDT_MOD_SRC_LINE
	type_t			type;		// UDT's type index
	ItemId			src;		// index into string table where source file name is saved
	uint32			line;		// line number
	uint16			imod;		// module that contributes this UDT definition
};

// build information
struct BuildInfo : Leaf { // LF_BUILDINFO
	enum index {
		CurrentDirectory 	= 0,
		BuildTool 			= 1,	// Cl.exe
		SourceFile 			= 2,	// foo.cpp
		ProgramDatabaseFile	= 3,	// foo.pdb
		CommandArguments 	= 4,	// -I etc
		KNOWN
	};
	uint16		count;		// number of arguments
	ItemId		arg[KNOWN];	// arguments as CodeItemId
};

//	MANAGED
struct Managed : Leaf { // LF_MANAGED
	embedded_string	name;		// utf8, zero terminated managed type name
};

//	ENUM
struct Enum_16t : Leaf { // LF_ENUM_16t
	uint16			count;		// count of number of elements in class
	type16_t		utype;		// underlying type of the enum
	type16_t		field;		// type index of LF_FIELD descriptor list
	prop_t			property;	// property attribute field
	embedded_string	name;		// length prefixed name of enum
};
struct Enum : Leaf { // LF_ENUM
	uint16			count;		// count of number of elements in class
	prop_t			property;	// property attribute field
	type_t			utype;		// underlying type of the enum
	type_t			field;		// type index of LF_FIELD descriptor list
	embedded_string	name;		// length prefixed name of enum
};

//	PROCEDURE
struct Proc_16t : Leaf { // LF_PROCEDURE_16t
	type16_t		rvtype;		// type index of return value
	uint8			calltype;	// calling convention (call_t)
	funcattr_t		funcattr;	// attributes
	uint16			parmcount;	// number of parameters
	type16_t		arglist;	// type index of argument list
};
struct Proc : Leaf { // LF_PROCEDURE
	type_t			rvtype;		// type index of return value
	uint8			calltype;	// calling convention (call_t)
	funcattr_t		funcattr;	// attributes
	uint16			parmcount;	// number of parameters
	type_t			arglist;	// type index of argument list
};

//	member function
struct MFunc_16t : Leaf { // LF_MFUNCTION_16t
	type16_t		rvtype;		// type index of return value
	type16_t		classtype;	// type index of containing class
	type16_t		thistype;	// type index of this pointer (model specific)
	uint8			calltype;	// calling convention (call_t)
	funcattr_t		funcattr;	// attributes
	uint16			parmcount;	// number of parameters
	type16_t		arglist;	// type index of argument list
	long			thisadjust;	// this adjuster (long because pad required anyway)
};
struct MFunc : Leaf { // LF_MFUNCTION
	type_t			rvtype;		// type index of return value
	type_t			classtype;	// type index of containing class
	type_t			thistype;	// type index of this pointer (model specific)
	uint8			calltype;	// calling convention (call_t)
	funcattr_t		funcattr;	// attributes
	uint16			parmcount;	// number of parameters
	type_t			arglist;	// type index of argument list
	long			thisadjust;	// this adjuster (long because pad required anyway)
};

//	virtual function table shape
struct VTShape : Leaf { // LF_VTSHAPE
//	enumeration for virtual shape table entries
	enum desc {
		Near		= 0x00,
		Far			= 0x01,
		Thin		= 0x02,
		Outer		= 0x03,
		Meta		= 0x04,
		Near32		= 0x05,
		Far32		= 0x06,
		Unused		= 0x07
	};
	uint16			count;		// number of entries in vfunctable
	uint8			desc[];		// 4 bit (desc) descriptors
};

//	virtual function table
struct Vftable : Leaf { // LF_VFTABLE
	type_t		type;		// class/structure that owns the vftable
	type_t		baseVftable;			// vftable from which this vftable is derived
	uint32		offsetInObjectLayout;	// offset of the vfptr to this table, relative to the start of the object layout
	uint32		len;		// length of the Names array below in bytes
	uint8		Names[1];	// array of names
	// The first is the name of the vtable, the others are the names of the methods
	// TS-TODO: replace a name with a NamedCodeItem once Weiping is done, to avoid duplication of method names
};

//	COBOL
struct Cobol0_16t : Leaf { // LF_COBOL0_16t
	type16_t	type;		// parent type record index
	uint8		data[];
};
struct Cobol0 : Leaf { // LF_COBOL0
	type_t		type;		// parent type record index
	uint8		data[];
};

struct Cobol1 : Leaf { // LF_COBOL1
	uint8		data[];
};

//	basic array
struct BArray_16t : Leaf { // LF_BARRAY_16t
	type16_t	utype;		// type index of underlying type
};
struct BArray : Leaf { // LF_BARRAY
	type_t		utype;		// type index of underlying type
};

//	assembler labels
struct Label : Leaf { // LF_LABEL
	enum MODE {
		Near		= 0,	// near return
		Far			= 4		// far return
	};
	uint16		mode;		// addressing mode of label
};

//	dimensioned arrays
struct DimArray_16t : Leaf { // LF_DIMARRAY_16t
	type16_t		utype;		// underlying type of the array
	type16_t		diminfo;	// dimension information
	embedded_string	name;		// length prefixed name
};
struct DimArray : Leaf { // LF_DIMARRAY
	type_t			utype;		// underlying type of the array
	type_t			diminfo;	// dimension information
	embedded_string	name;		// length prefixed name
};

//	path to virtual function table
struct VFTPath_16t : Leaf { // LF_VFTPATH_16t
	uint16			count;		// count of number of bases in path
	type16_t		base[1];	// bases from root to leaf
};
struct VFTPath : Leaf { // LF_VFTPATH
	uint32			count;		// count of number of bases in path
	type_t			base[1];	// bases from root to leaf
};

//	inclusion of precompiled types
struct PreComp_16t : Leaf { // LF_PRECOMP_16t
	uint16			start;		// starting type index included
	uint16			count;		// number of types in inclusion
	uint32			signature;	// signature
	embedded_string	name;		// length prefixed name of included type file
};
struct PreComp : Leaf { // LF_PRECOMP
	uint32			start;		// starting type index included
	uint32			count;		// number of types in inclusion
	uint32			signature;	// signature
	embedded_string	name;		// length prefixed name of included type file
};

//	end of precompiled types that can be included by another file
struct EndPreComp : Leaf { // LF_ENDPRECOMP
	uint32			signature;	// signature
};

//	OEM definable type strings
struct OEM_16t : Leaf { // LF_OEM_16t
	uint16			cvOEM;		// MS assigned OEM identified
	uint16			recOEM;		// OEM assigned type identifier
	uint16			count;		// count of type indices to follow
	type16_t			index[];	// array of type indices followed by OEM defined data
};
struct OEM : Leaf { // LF_OEM
	uint16			cvOEM;		// MS assigned OEM identified
	uint16			recOEM;		// OEM assigned type identifier
	uint32			count;		// count of type indices to follow
	type_t			index[];	// array of type indices followed by OEM defined data
};

#define OEM_MS_FORTRAN90		0xF090
#define OEM_ODI					0x0010
#define OEM_THOMSON_SOFTWARE	0x5453
#define OEM_ODI_REC_BASELIST	0x0000

struct OEM2 : Leaf { // LF_OEM2
	uint8			idOem[16];	// an oem ID (GUID)
	uint32			count;		// count of type indices to follow
	type_t			index[];	// array of type indices followed by OEM defined data
};

// type record describing using of a type server
struct TypeServer : Leaf { // LF_TYPESERVER
	uint32			signature;	// signature
	uint32			age;		// age of database used by this module
	embedded_string	name;		// length prefixed name of PDB
};

// type record describing using of a type server with v7 (GUID) signatures
struct TypeServer2 : Leaf { // LF_TYPESERVER2
	GUID			sig70;		// guid signature
	uint32			age;		// age of database used by this module
	embedded_string	name;		// length prefixed name of PDB
};

// description of type records that can be referenced from type records referenced by symbols type record for skip record
struct Skip_16t : Leaf { // LF_SKIP_16t
	type16_t		type;		// next valid index
	uint8			data[];		// pad data
};
struct Skip : Leaf { // LF_SKIP
	type_t			type;		// next valid index
	uint8			data[];		// pad data
};

//	argument list leaf
struct ArgList_16t : Leaf { // LF_ARGLIST_16t
	uint16			count;		// number of arguments
	type16_t		arg[];		// number of arguments
	auto	list() const { return make_range_n(arg, count); }
};
struct ArgList : Leaf { // LF_ARGLIST, LF_SUBSTR_LIST
	uint32			count;		// number of arguments
	type_t			arg[];		// number of arguments
	auto	list() const { return make_range_n(arg, count); }
};

//	derived class list leaf
struct Derived_16t : Leaf { // LF_DERIVED_16t
	uint16			count;		// number of arguments
	type16_t		drvdcls[];	// type indices of derived classes
	auto	list() const { return make_range_n(drvdcls, count); }
};
struct Derived : Leaf { // LF_DERIVED
	uint32			count;		// number of arguments
	type_t			drvdcls[];	// type indices of derived classes
	auto	list() const { return make_range_n(drvdcls, count); }
};

//	leaf for default arguments
struct DefArg_16t : Leaf { // LF_DEFARG_16t
	type16_t		type;		// type of resulting expression
	uint8			expr[];		// length prefixed expression string
};
struct DefArg : Leaf { // LF_DEFARG
	type_t			type;		// type of resulting expression
	uint8			expr[];		// length prefixed expression string
};

//	list leaf
// This list should no longer be used because the utilities cannot verify the contents of the list without knowing what type of list it is (new specific leaf indices should be used instead)
struct List : Leaf { // LF_LIST
	char			data[];	// data format specified by indexing type
};

// header leaf for a complex list of class and structure subfields
struct FieldList_16t : Leaf { // LF_FIELDLIST_16t
	Leaf			data[];	// field list sub lists
	auto	list() const { return make_next_range(data, (const Leaf*)(void*)as<TYPTYPE>()->next()); }
};
typedef FieldList_16t FieldList;	// LF_FIELDLIST

struct MethodList_16t : Leaf {
	struct entry {
		fldattr_t		attr;			// method attribute
		type16_t		index;			// index to type record for procedure
		uint32			vbaseoff[1];	// offset in vfunctable if intro virtual
		const entry	*next()	const { return (const entry*)&vbaseoff[attr.extra()]; }
	};
	entry			mList[];
	auto	list() const { return make_next_range(mList, (const entry*)as<TYPTYPE>()->next()); }
};
struct MethodList : Leaf {
	//	non-static methods and friends in overloaded method list
	struct entry {
		fldattr_t		attr;			// method attribute
		uint16			pad0;			// internal padding, must be 0
		type_t			index;			// index to type record for procedure
		uint32			vbaseoff[1];	// offset in vfunctable if intro virtual
		const entry	*next()	const { return (const entry*)&vbaseoff[attr.extra()]; }
	};
	entry			mList[];
	auto	list() const { return make_next_range(mList, (const entry*)as<TYPTYPE>()->next()); }
};

//	BITFIELD
struct Bitfield_16t : Leaf { // LF_BITFIELD_16t
	uint8			length;
	uint8			position;
	type16_t		type;		// type of bitfield
};
struct Bitfield : Leaf { // LF_BITFIELD
	type_t			type;		// type of bitfield
	uint8			length;
	uint8			position;
};

//	dimensioned array with constant bounds
struct DimCon_16t : Leaf { // LF_DIMCONU_16t or LF_DIMCONLU_16t
	uint16			rank;		// number of dimensions
	type16_t		typ;		// type of index
	uint8			dim[];		// array of dimension information with either upper bounds or lower/upper bound
};
struct DimCon : Leaf { // LF_DIMCONU or LF_DIMCONLU
	type_t			typ;		// type of index
	uint16			rank;		// number of dimensions
	uint8			dim[];		// array of dimension information with either upper bounds or lower/upper bound
};

//	dimensioned array with variable bounds
struct DimVar_16t : Leaf { // LF_DIMVARU_16t or LF_DIMVARLU_16t
	uint16			rank;		// number of dimensions
	type16_t		typ;		// type of index
	type16_t		dim[];		// array of type indices for either variable upper bound or variable lower/upper bound;	referenced types must be LF_REFSYM or T_VOID
};
struct DimVar : Leaf { // LF_DIMVARU or LF_DIMVARLU
	uint32			rank;		// number of dimensions
	type_t			typ;		// type of index
	type_t			dim[];		// array of type indices for either variable upper bound or variable
	// lower/upper bound.	The count of type indices is rank or rank*2 depending on whether it is LFDIMVARU or LF_DIMVARLU; referenced types must be LF_REFSYM or T_VOID
};

//	referenced symbol
struct RefSym : Leaf { // LF_REFSYM
	uint8			Sym[1];		// copy of referenced symbol record (including length)
};

//	generic HLSL type
struct HLSL : Leaf { // LF_HLSL
	type_t			subtype;		// sub-type index, if any
	uint16			kind;			// kind of built-in type from builtin_e
	uint16			numprops : 4;	// number of numeric properties
	uint16			unused : 12;	// padding, must be 0
	uint8			data[];			// variable-length array of numeric properties followed by byte size
	size_t	type_size()	const	{ return data[numprops]; }
	auto	props()		const	{ return make_range_n(data, numprops); }
};

//	a generalized built-in type modifier
struct ModifierEx : Leaf { // LF_MODIFIER_EX
	type_t			type;		// type being modified
	uint16			count;		// count of modifier values
	uint16			mods[];		// modifiers from modifier_e
	auto	list()		const	{ return make_range_n(mods, count); }
};

// index leaf - contains type index of another leaf
// used to allow the compilers to emit a long complex list (LF_FIELD) in smaller pieces
struct Index_16t : Leaf { // LF_INDEX_16t
	type16_t		index;		// type index of referenced leaf
};
struct Index : Leaf { // LF_INDEX
	uint16			pad0;		// internal padding, must be 0
	type_t			index;		// type index of referenced leaf
};

//	base class field
struct BClass_16t : Leaf { // LF_BCLASS_16t
	type16_t			index;		// type index of base class
	fldattr_t		attr;		// attribute
	Value			offset;		// offset of base within class
	const void*		after() const { return offset.after(); }
};
struct BClass : Leaf { // LF_BCLASS, LF_BINTERFACE
	fldattr_t		attr;		// attribute
	type_t			index;		// type index of base class
	Value			offset;		// offset of base within class
	const void*		after() const { return offset.after(); }
};
typedef BClass BInterface;

//	direct and indirect virtual base class field
struct VBClass_16t : Leaf { // LF_VBCLASS_16t | LV_IVBCLASS_16t
	type16_t		index;		// type index of direct virtual base class
	type16_t		vbptr;		// type index of virtual base pointer
	fldattr_t		attr;		// attribute
	Value			vbpoff;		// virtual base pointer offset from address point followed by virtual base offset from vbtable
	const Value&	vbboff() const	{ return *(Value*)vbpoff.after(); }
	const void*		after()	const	{ return vbboff().after(); }
};
struct VBClass : Leaf { // LF_VBCLASS | LV_IVBCLASS
	fldattr_t		attr;		// attribute
	type_t			index;		// type index of direct virtual base class
	type_t			vbptr;		// type index of virtual base pointer
	Value			vbpoff;		// virtual base pointer offset from address point followed by virtual base offset from vbtable
	const Value&	vbboff() const	{ return *(Value*)vbpoff.after(); }
	const void*		after()	const	{ return vbboff().after(); }
};

//	friend class
struct FriendCls_16t : Leaf { // LF_FRIENDCLS_16t
	type16_t		index;		// index to type record of friend class
};
struct FriendCls : Leaf { // LF_FRIENDCLS
	uint16			pad0;		// internal padding, must be 0
	type_t			index;		// index to type record of friend class
};

//	friend function
struct FriendFcn_16t : Leaf { // LF_FRIENDFCN_16t
	type16_t			index;		// index to type record of friend function
	embedded_string	name;		// name of friend function
	const void*		after() const { return name.end() + 1; }
};
struct FriendFcn : Leaf { // LF_FRIENDFCN
	uint16			pad0;		// internal padding, must be 0
	type_t			index;		// index to type record of friend function
	embedded_string	name;		// name of friend function
};

//	non-static data members
struct Member_16t : Leaf { // LF_MEMBER_16t
	type16_t		index;		// index of field
	fldattr_t		attr;		// attribute mask
	Value			offset;		// offset of field
	const char*		name()	const	{ return offset.after(); }
	const void*		after() const	{ return str(name()).end() + 1; }
};
struct Member : Leaf { // LF_MEMBER
	fldattr_t		attr;		// attribute mask
	type_t			index;		// index of type record for field
	Value			offset;		// offset of field
	const char*		name()	const	{ return offset.after(); }
	const void*		after() const	{ return str(name()).end() + 1; }
};

//	static data members
struct STMember_16t : Leaf { // LF_STMEMBER_16t
	type16_t		index;		// index of type record for field
	fldattr_t		attr;		// attribute mask
	embedded_string	name;		// length prefixed name of field
	const void*		after() const	{ return name.end() + 1; }
};
struct STMember : Leaf { // LF_STMEMBER
	fldattr_t		attr;		// attribute mask
	type_t			index;		// index of type record for field
	embedded_string	name;		// length prefixed name of field
	const void*		after() const	{ return name.end() + 1; }
};

//	virtual function table pointer
struct VFuncTab_16t : Leaf { // LF_VFUNCTAB_16t
	type16_t		type;	// type index of pointer
};
struct VFuncTab : Leaf { // LF_VFUNCTAB
	uint16			pad0;		// internal padding, must be 0
	type_t			type;		// type index of pointer
};

//	virtual function table pointer with offset
struct VFuncOff_16t : Leaf { // LF_VFUNCOFF_16t
	type16_t			type;		// type index of pointer
	int32			offset;		// offset of virtual function table pointer
};
struct VFuncOff : Leaf { // LF_VFUNCOFF
	uint16			pad0;		// internal padding, must be 0
	type_t			type;		// type index of pointer
	int32			offset;		// offset of virtual function table pointer
};

//	overloaded method list
struct Method_16t : Leaf { // LF_METHOD_16t
	uint16			count;		// number of occurrences of function
	type16_t		mList;		// index to LF_METHODLIST record
	embedded_string	name;		// length prefixed name of method
	const void*		after() const	{ return name.end() + 1; }
};
struct Method : Leaf { // LF_METHOD
	uint16			count;		// number of occurrences of function
	type_t			mList;		// index to LF_METHODLIST record
	embedded_string	name;		// length prefixed name of method
	const void*		after() const	{ return name.end() + 1; }
};

//	nonoverloaded method
struct OneMethod_16t : Leaf { // LF_ONEMETHOD_16t
	fldattr_t		attr;		// method attribute
	type16_t		index;		// index to type record for procedure
	uint32			vbaseoff[];	// offset in vfunctable if intro virtual followed by length prefixed name of method
	const char*		name()	const	{ return (const char*)(vbaseoff + attr.extra()); }
	const void*		after() const	{ return str(name()).end() + 1; }
};
struct OneMethod : Leaf { // LF_ONEMETHOD
	fldattr_t		attr;		// method attribute
	type_t			index;		// index to type record for procedure
	uint32			vbaseoff[];	// offset in vfunctable if intro virtual followed by length prefixed name of method
	const char*		name()	const	{ return (const char*)(vbaseoff + attr.extra()); }
	const void*		after() const	{ return str(name()).end() + 1; }
};

//	enumerate
struct Enumerate : Leaf { // LF_ENUMERATE
	fldattr_t		attr;		// access
	Value			value;
	const char*		name()	const	{ return value.after(); }
	const void*		after() const	{ return str(name()).end() + 1; }
};

//	nested (scoped) type definition
struct NestType_16t : Leaf { // LF_NESTTYPE_16t
	type16_t		index;		// index of nested type definition
	embedded_string	name;		// length prefixed type name
	const void*		after() const	{ return name.end() + 1; }
};
struct NestType : Leaf { // LF_NESTTYPE
	uint16			pad0;		// internal padding, must be 0
	type_t			index;		// index of nested type definition
	embedded_string	name;		// length prefixed type name
	const void*		after() const	{ return name.end() + 1; }
};

//	nested (scoped) type definition, with attributes
//	new records for vC v5.0, no need to have 16-bit ti versions
struct NestTypeEx : Leaf { // LF_NESTTYPEEX
	fldattr_t		attr;		// member access
	type_t			index;		// index of nested type definition
	embedded_string	name;		// length prefixed type name
	const void*		after() const	{ return name.end() + 1; }
};

//	modifications to members
struct MemberModify : Leaf { // LF_MEMBERMODIFY
	fldattr_t		attr;		// the new attributes
	type_t			index;		// index of base class type definition
	embedded_string	name;		// length prefixed member name
	const void*		after() const	{ return name.end() + 1; }
};

inline const Leaf *next(const Leaf *i) {
	const void *a;
	switch (i->leaf) {
		case LF_ENUMERATE:		a = i->as<Enumerate>()->after();	break;

		// 32 bit
		case LF_INDEX:			a = i->as<Index>() + 1;				break;
		case LF_VFUNCTAB:		a = i->as<VFuncTab>()	+ 1;		break;
		case LF_FRIENDCLS:		a = i->as<FriendCls>() + 1;			break;
		case LF_VFUNCOFF:		a = i->as<VFuncOff>() + 1;			break;

		case LF_MEMBER:			a = i->as<Member>()->after();		break;
		case LF_STMEMBER:		a = i->as<STMember>()->after();		break;
		case LF_METHOD:			a = i->as<Method>()->after();		break;
		case LF_NESTTYPE:		a = i->as<NestType>()->after();		break;
		case LF_ONEMETHOD:		a = i->as<OneMethod>()->after();	break;
		case LF_BCLASS:
		case LF_BINTERFACE:		a = i->as<BClass>()->after();		break;
		case LF_VBCLASS:		a = i->as<VBClass>()->after();		break;
		case LF_IVBCLASS:		a = i->as<VBClass>()->after();		break;
		case LF_FRIENDFCN:		a = i->as<Method>()->after();		break;

		// 16 bit
		case LF_INDEX_16t:		a = i->as<Index_16t>() + 1;			break;
		case LF_VFUNCTAB_16t:	a = i->as<VFuncTab_16t>()	+ 1;	break;
		case LF_FRIENDCLS_16t:	a = i->as<FriendCls_16t>() + 1;		break;
		case LF_VFUNCOFF_16t:	a = i->as<VFuncOff_16t>() + 1;		break;

		case LF_MEMBER_16t:		a = i->as<Member_16t>()->after();	break;
		case LF_STMEMBER_16t:	a = i->as<STMember_16t>()->after();	break;
		case LF_METHOD_16t:		a = i->as<Method_16t>()->after();	break;
		case LF_NESTTYPE_16t:	a = i->as<NestType_16t>()->after();	break;
		case LF_ONEMETHOD_16t:	a = i->as<OneMethod_16t>()->after();break;
		case LF_BCLASS_16t:		a = i->as<BClass_16t>()->after();	break;
		case LF_VBCLASS_16t:	a = i->as<VBClass_16t>()->after();	break;
		case LF_IVBCLASS_16t:	a = i->as<VBClass>()->after();		break;
		case LF_FRIENDFCN_16t:	a = i->as<Method_16t>()->after();	break;

		default:				return 0;
	}
	if (!is_aligned(a, 4) && *(uint8*)a > LF_PAD0)
		a = (uint8*)a + *(uint8*)a - LF_PAD0;
	return (const Leaf*)a;
}

//-----------------------------------------------------------------------------
//	Symbol definitions
//-----------------------------------------------------------------------------

enum SYM_ENUM_e : uint16 {
	S_COMPILE				= 0x0001,	// Compile flags symbol
	S_REGISTER_16t			= 0x0002,	// Register variable
	S_CONSTANT_16t			= 0x0003,	// constant symbol
	S_UDT_16t				= 0x0004,	// User defined type
	S_SSEARCH				= 0x0005,	// Start Search
	S_END					= 0x0006,	// Block, procedure, "with" or thunk end
	S_SKIP					= 0x0007,	// Reserve symbol space in $$Symbols table
	S_CVRESERVE				= 0x0008,	// Reserved symbol for CV internal use
	S_OBJNAME_ST			= 0x0009,	// path to object file name
	S_ENDARG				= 0x000a,	// end of argument/return list
	S_COBOLUDT_16t			= 0x000b,	// special UDT for cobol that does not symbol pack
	S_MANYREG_16t			= 0x000c,	// multiple register variable
	S_RETURN				= 0x000d,	// return description symbol
	S_ENTRYTHIS				= 0x000e,	// description of this pointer on entry
	S_BPREL16				= 0x0100,	// BP-relative
	S_LDATA16				= 0x0101,	// Module-local symbol
	S_GDATA16				= 0x0102,	// Global data symbol
	S_PUB16					= 0x0103,	// a public symbol
	S_LPROC16				= 0x0104,	// Local procedure start
	S_GPROC16				= 0x0105,	// Global procedure start
	S_THUNK16				= 0x0106,	// Thunk Start
	S_BLOCK16				= 0x0107,	// block start
	S_WITH16				= 0x0108,	// with start
	S_LABEL16				= 0x0109,	// code label
	S_CEXMODEL16			= 0x010a,	// change execution model
	S_VFTABLE16				= 0x010b,	// address of virtual function table
	S_REGREL16				= 0x010c,	// register relative address
	S_BPREL32_16t			= 0x0200,	// BP-relative
	S_LDATA32_16t			= 0x0201,	// Module-local symbol
	S_GDATA32_16t			= 0x0202,	// Global data symbol
	S_PUB32_16t				= 0x0203,	// a public symbol (CV internal reserved)
	S_LPROC32_16t			= 0x0204,	// Local procedure start
	S_GPROC32_16t			= 0x0205,	// Global procedure start
	S_THUNK32_ST			= 0x0206,	// Thunk Start
	S_BLOCK32_ST			= 0x0207,	// block start
	S_WITH32_ST				= 0x0208,	// with start
	S_LABEL32_ST			= 0x0209,	// code label
	S_CEXMODEL32			= 0x020a,	// change execution model
	S_VFTABLE32_16t			= 0x020b,	// address of virtual function table
	S_REGREL32_16t			= 0x020c,	// register relative address
	S_LTHREAD32_16t			= 0x020d,	// local thread storage
	S_GTHREAD32_16t			= 0x020e,	// global thread storage
	S_SLINK32				= 0x020f,	// static link for MIPS EH implementation
	S_LPROCMIPS_16t			= 0x0300,	// Local procedure start
	S_GPROCMIPS_16t			= 0x0301,	// Global procedure start

	// if these ref symbols have names following then the names are in ST format
	S_PROCREF_ST			= 0x0400,	// Reference to a procedure
	S_DATAREF_ST			= 0x0401,	// Reference to data
	S_ALIGN					= 0x0402,	// Used for page alignment of symbols
	S_LPROCREF_ST			= 0x0403,	// Local Reference to a procedure
	S_OEM					= 0x0404,	// OEM defined symbol

	// sym records with 32-bit types embedded instead of 16-bit all have 0x1000 bit set for easy identification
	S_TI16_MAX				= 0x1000,
	S_REGISTER_ST			= 0x1001,	// Register variable
	S_CONSTANT_ST			= 0x1002,	// constant symbol
	S_UDT_ST				= 0x1003,	// User defined type
	S_COBOLUDT_ST			= 0x1004,	// special UDT for cobol that does not symbol pack
	S_MANYREG_ST			= 0x1005,	// multiple register variable
	S_BPREL32_ST			= 0x1006,	// BP-relative
	S_LDATA32_ST			= 0x1007,	// Module-local symbol
	S_GDATA32_ST			= 0x1008,	// Global data symbol
	S_PUB32_ST				= 0x1009,	// a public symbol (CV internal reserved)
	S_LPROC32_ST			= 0x100a,	// Local procedure start
	S_GPROC32_ST			= 0x100b,	// Global procedure start
	S_VFTABLE32				= 0x100c,	// address of virtual function table
	S_REGREL32_ST			= 0x100d,	// register relative address
	S_LTHREAD32_ST			= 0x100e,	// local thread storage
	S_GTHREAD32_ST			= 0x100f,	// global thread storage
	S_LPROCMIPS_ST			= 0x1010,	// Local procedure start
	S_GPROCMIPS_ST			= 0x1011,	// Global procedure start
	S_FRAMEPROC				= 0x1012,	// extra frame and proc information
	S_COMPILE2_ST			= 0x1013,	// extended compile flags and info
	// new symbols necessary for 16-bit enumerates of IA64 registers and IA64 specific symbols
	S_MANYREG2_ST			= 0x1014,	// multiple register variable
	S_LPROCIA64_ST			= 0x1015,	// Local procedure start (IA64)
	S_GPROCIA64_ST			= 0x1016,	// Global procedure start (IA64)
	// Local symbols for IL
	S_LOCALSLOT_ST			= 0x1017,	// local IL sym with field for local slot index
	S_PARAMSLOT_ST			= 0x1018,	// local IL sym with field for parameter slot index
	S_ANNOTATION			= 0x1019,	// Annotation string literals
	// symbols to support managed code debugging
	S_GMANPROC_ST			= 0x101a,	// Global proc
	S_LMANPROC_ST			= 0x101b,	// Local proc
	S_RESERVED1				= 0x101c,	// reserved
	S_RESERVED2				= 0x101d,	// reserved
	S_RESERVED3				= 0x101e,	// reserved
	S_RESERVED4				= 0x101f,	// reserved
	S_LMANDATA_ST			= 0x1020,
	S_GMANDATA_ST			= 0x1021,
	S_MANFRAMEREL_ST		= 0x1022,
	S_MANREGISTER_ST		= 0x1023,
	S_MANSLOT_ST			= 0x1024,
	S_MANMANYREG_ST			= 0x1025,
	S_MANREGREL_ST			= 0x1026,
	S_MANMANYREG2_ST		= 0x1027,
	S_MANTYPREF				= 0x1028,	// Index for type referenced by name from metadata
	S_UNAMESPACE_ST			= 0x1029,	// Using namespace

	// Symbols w/ SZ name fields. All name fields contain utf8 encoded strings
	S_ST_MAX				= 0x1100,	// starting point for SZ name symbols
	S_OBJNAME				= 0x1101,	// path to object file name
	S_THUNK32				= 0x1102,	// Thunk Start
	S_BLOCK32				= 0x1103,	// block start
	S_WITH32				= 0x1104,	// with start
	S_LABEL32				= 0x1105,	// code label
	S_REGISTER				= 0x1106,	// Register variable
	S_CONSTANT				= 0x1107,	// constant symbol
	S_UDT					= 0x1108,	// User defined type
	S_COBOLUDT				= 0x1109,	// special UDT for cobol that does not symbol pack
	S_MANYREG				= 0x110a,	// multiple register variable
	S_BPREL32				= 0x110b,	// BP-relative
	S_LDATA32				= 0x110c,	// Module-local symbol
	S_GDATA32				= 0x110d,	// Global data symbol
	S_PUB32					= 0x110e,	// a public symbol (CV internal reserved)
	S_LPROC32				= 0x110f,	// Local procedure start
	S_GPROC32				= 0x1110,	// Global procedure start
	S_REGREL32				= 0x1111,	// register relative address
	S_LTHREAD32				= 0x1112,	// local thread storage
	S_GTHREAD32				= 0x1113,	// global thread storage
	S_LPROCMIPS				= 0x1114,	// Local procedure start
	S_GPROCMIPS				= 0x1115,	// Global procedure start
	S_COMPILE2				= 0x1116,	// extended compile flags and info
	S_MANYREG2				= 0x1117,	// multiple register variable
	S_LPROCIA64				= 0x1118,	// Local procedure start (IA64)
	S_GPROCIA64				= 0x1119,	// Global procedure start (IA64)
	S_LOCALSLOT				= 0x111a,	// local IL sym with field for local slot index
	S_SLOT					= S_LOCALSLOT,	// alias for LOCALSLOT
	S_PARAMSLOT				= 0x111b,	// local IL sym with field for parameter slot index

	// symbols to support managed code debugging
	S_LMANDATA				= 0x111c,
	S_GMANDATA				= 0x111d,
	S_MANFRAMEREL			= 0x111e,
	S_MANREGISTER			= 0x111f,
	S_MANSLOT				= 0x1120,
	S_MANMANYREG			= 0x1121,
	S_MANREGREL				= 0x1122,
	S_MANMANYREG2			= 0x1123,
	S_UNAMESPACE			= 0x1124,	// Using namespace

	// ref symbols with name fields
	S_PROCREF				= 0x1125,	// Reference to a procedure
	S_DATAREF				= 0x1126,	// Reference to data
	S_LPROCREF				= 0x1127,	// Local Reference to a procedure
	S_ANNOTATIONREF			= 0x1128,	// Reference to an S_ANNOTATION symbol
	S_TOKENREF				= 0x1129,	// Reference to one of the many MANPROCSYM's

	// continuation of managed symbols
	S_GMANPROC				= 0x112a,	// Global proc
	S_LMANPROC				= 0x112b,	// Local proc
	// int16, light-weight thunks
	S_TRAMPOLINE			= 0x112c,	// trampoline thunks
	S_MANCONSTANT			= 0x112d,	// constants with metadata type info
	// native attributed local/parms
	S_ATTR_FRAMEREL			= 0x112e,	// relative to virtual frame ptr
	S_ATTR_REGISTER			= 0x112f,	// stored in a register
	S_ATTR_REGREL			= 0x1130,	// relative to register (alternate frame ptr)
	S_ATTR_MANYREG			= 0x1131,	// stored in >1 register
	// Separated code (from the compiler) support
	S_SEPCODE				= 0x1132,
	S_LOCAL_2005			= 0x1133,	// defines a local symbol in optimized code
	S_DEFRANGE_2005			= 0x1134,	// defines a single range of addresses in which symbol can be evaluated
	S_DEFRANGE2_2005		= 0x1135,	// defines ranges of addresses in which symbol can be evaluated
	S_SECTION				= 0x1136,	// A COFF section in a PE executable
	S_COFFGROUP				= 0x1137,	// A COFF group
	S_EXPORT				= 0x1138,	// A export
	S_CALLSITEINFO			= 0x1139,	// Indirect call site information
	S_FRAMECOOKIE			= 0x113a,	// Security cookie information
	S_DISCARDED				= 0x113b,	// Discarded by LINK /OPT:REF (experimental, see richards)
	S_COMPILE3				= 0x113c,	// Replacement for S_COMPILE2
	S_ENVBLOCK				= 0x113d,	// Environment block split off from S_COMPILE2
	S_LOCAL					= 0x113e,	// defines a local symbol in optimized code
	S_DEFRANGE				= 0x113f,	// defines a single range of addresses in which symbol can be evaluated
	S_DEFRANGE_SUBFIELD		= 0x1140,	// ranges for a subfield
	S_DEFRANGE_REGISTER		= 0x1141,	// ranges for en-registered symbol

	S_DEFRANGE_FRAMEPOINTER_REL				= 0x1142,	// range for stack symbol
	S_DEFRANGE_SUBFIELD_REGISTER			= 0x1143,	// ranges for en-registered field of symbol
	S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE	= 0x1144,	// range for stack symbol span valid full scope of function body, gap might apply
	S_DEFRANGE_REGISTER_REL					= 0x1145,	// range for symbol address as register + offset
	// S_PROC symbols that reference ID instead of type
	S_LPROC32_ID			= 0x1146,
	S_GPROC32_ID			= 0x1147,
	S_LPROCMIPS_ID			= 0x1148,
	S_GPROCMIPS_ID			= 0x1149,
	S_LPROCIA64_ID			= 0x114a,
	S_GPROCIA64_ID			= 0x114b,
	S_BUILDINFO				= 0x114c,	// build information
	S_INLINESITE			= 0x114d,	// inlined function callsite
	S_INLINESITE_END		= 0x114e,
	S_PROC_ID_END			= 0x114f,
	S_DEFRANGE_HLSL			= 0x1150,
	S_GDATA_HLSL			= 0x1151,
	S_LDATA_HLSL			= 0x1152,
	S_FILESTATIC			= 0x1153,
#if defined(CC_DP_CXX) && CC_DP_CXX
	S_LOCAL_DPC_GROUPSHARED	= 0x1154,	// DPC groupshared variable
	S_LPROC32_DPC			= 0x1155,	// DPC local procedure start
	S_LPROC32_DPC_ID		= 0x1156,
	S_DEFRANGE_DPC_PTR_TAG	= 0x1157,	// DPC pointer tag definition range
	S_DPC_SYM_TAG_MAP		= 0x1158,	// DPC pointer tag value to symbol record map
#endif // CC_DP_CXX
	S_ARMSWITCHTABLE		= 0x1159,
	S_CALLEES				= 0x115a,
	S_CALLERS				= 0x115b,
	S_POGODATA				= 0x115c,
	S_INLINESITE2			= 0x115d,	// extended inline site information
	S_HEAPALLOCSITE			= 0x115e,	// heap allocation site
	S_MOD_TYPEREF			= 0x115f,	// only generated at link time
	S_REF_MINIPDB			= 0x1160,	// only generated at link time for mini PDB
	S_PDBMAP				= 0x1161,	// only generated at link time for mini PDB
	S_GDATA_HLSL32			= 0x1162,
	S_LDATA_HLSL32			= 0x1163,
	S_GDATA_HLSL32_EX		= 0x1164,
	S_LDATA_HLSL32_EX		= 0x1165,

	S_FASTLINK				= 0x1167, // Undocumented
	S_INLINEES				= 0x1168, // Undocumented

	S_RECTYPE_MAX,						// one greater than last
	S_RECTYPE_LAST			= S_RECTYPE_MAX - 1,
	S_RECTYPE_PAD			= S_RECTYPE_MAX + 0x100 // Used *only* to verify symbol record types so that current PDB code can potentially read future PDBs (assuming no format change, etc)
};

struct LineNumberParser;

// enum describing function return method
struct PROCFLAGS {
	union {
		uint8		bAll;
		struct {
			uint8 PFLAG_NOFPO		: 1;	// frame pointer present
			uint8 PFLAG_INT			: 1;	// interrupt return
			uint8 PFLAG_FAR			: 1;	// far return
			uint8 PFLAG_NEVER		: 1;	// function does not return
			uint8 PFLAG_NOTREACHED	: 1;	// label isn't fallen into
			uint8 PFLAG_CUST_CALL	: 1;	// custom calling convention
			uint8 PFLAG_NOINLINE	: 1;	// function marked as noinline
			uint8 PFLAG_OPTDBGINFO	: 1;	// function has debug information for optimized code
		};
	};
};

// Extended proc flags
struct EXPROCFLAGS {
	PROCFLAGS cvpf;
	union {
		uint8		grfAll;
		struct {
			uint8	__reserved_byte : 8;	// must be zero
		};
	};
};

// local variable flags
struct LVARFLAGS {
	uint16 fIsParam			: 1;	// variable is a parameter
	uint16 fAddrTaken		: 1;	// address is taken
	uint16 fCompGenx		: 1;	// variable is compiler generated
	uint16 fIsAggregate		: 1;	// the symbol is splitted in temporaries, which are treated by compiler as	independent entities
	uint16 fIsAggregated	: 1;	// Counterpart of fIsAggregate - tells that it is a part of a fIsAggregate symbol
	uint16 fIsAliased		: 1;	// variable has multiple simultaneous lifetimes
	uint16 fIsAlias			: 1;	// represents one of the multiple simultaneous lifetimes
	uint16 fIsRetValue		: 1;	// represents a function return value
	uint16 fIsOptimizedOut 	: 1;	// variable has no lifetimes
	uint16 fIsEnregGlob		: 1;	// variable is an enregistered global
	uint16 fIsEnregStat		: 1;	// variable is an enregistered static
	uint16 unused			: 5;	// must be zero
};

// extended attributes common to all local variables
struct lvar_attr {
	segmented32	addr;	// first code address where var is live
	LVARFLAGS	flags;	// local var flags
};

// This is max length of a lexical linear IP range
// The upper number are reserved for seeded and flow based range
#define CV_LEXICAL_RANGE_MAX	0xF000

// represents an address range, used for optimized code debug info
struct LVAR_ADDR_RANGE : segmented32 { // defines a range of addresses
	uint16		length;
};

struct LVAR_ADDR_RANGE2 {
	const LVAR_ADDR_RANGE	*start;
	const void				*end;

	// Represents the holes in overall address range, all address is pre-bbt.
	struct GAP {
		uint16	offset;		// relative offset from the beginning of the live range
		uint16	length;		// length of this gap
	};

	LVAR_ADDR_RANGE2(const LVAR_ADDR_RANGE *start, const void *end) : start(start), end(end) {}

	bool	test(segmented32 addr) {
		if (addr.seg != start->seg || addr.off < start->off)
			return false;

		uint32	rel = addr.off - start->off;
		if (rel >= start->length)
			return false;

		for (auto &gap : make_range((const GAP*)(start + 1), (const GAP*)end)) {
			if (rel >= gap.offset && rel < gap.offset + gap.length)
				return false;
		}
		return true;
	}
};

#if defined(CC_DP_CXX) && CC_DP_CXX
// Represents a mapping from a DPC pointer tag value to the corresponding symbol record
struct DPC_SYM_TAG_MAP_ENTRY {
	uint32 tagValue;	// address taken symbol's pointer tag value
	int32	symRecordOffset;	// offset of the symbol record from the S_LPROC32_DPC record it is nested within
};
#endif // CC_DP_CXX


// Generic layout for symbol records
struct SYMTYPE {
	uint16		reclen;	// Record length
	SYM_ENUM_e	rectyp;	// Record type
	const_memory_block	data()	const	{ return const_memory_block(this + 1, reclen - sizeof(uint16)); }
	SYMTYPE				*next()			{ return (SYMTYPE*)((uint8*)this + reclen + sizeof(uint16)); }
	const SYMTYPE		*next()	const	{ return (const SYMTYPE*)((const uint8*)this + reclen + sizeof(uint16)); }
	template<typename T> T*			as()		{ return static_cast<T*>(this); }
	template<typename T> const T*	as() const	{ return static_cast<const T*>(this); }
};

//	non-model specific symbol types
struct REGSYM_16t : SYMTYPE { // S_REGISTER_16t
	type16_t		typind;	// Type index
	uint16			reg;	// register enumerate
	embedded_string	name;	// Length-prefixed name
};
struct REGSYM : SYMTYPE { // S_REGISTER
	type_t			typind;	// Type index or Metadata token
	uint16			reg;	// register enumerate
	embedded_string	name;	// Length-prefixed name
};
struct ATTRREGSYM : SYMTYPE { // S_MANREGISTER | S_ATTR_REGISTER
	type_t			typind;	// Type index or Metadata token
	lvar_attr		attr;	// local var attributes
	uint16			reg;	// register enumerate
	embedded_string	name;	// Length-prefixed name
};
struct MANYREGSYM_16t : SYMTYPE { // S_MANYREG_16t
	type16_t		typind;	// Type index
	uint8			count;	// count of number of registers
	uint8			reg[1];	// count register enumerates followed by length-prefixed name.	Registers are most significant first
	const char*		name() const { return (const char*)(reg + count); }
};
struct MANYREGSYM : SYMTYPE { // S_MANYREG
	type_t			typind;	// Type index or metadata token
	uint8			count;	// count of number of registers
	uint8			reg[1];	// count register enumerates followed by length-prefixed name.	Registers are most significant first
	const char*		name() const { return (const char*)(reg + count); }
};
struct MANYREGSYM2 : SYMTYPE { // S_MANYREG2
	type_t			typind;	// Type index or metadata token
	uint16			count;	// count of number of registers
	uint16			reg[1];	// count register enumerates followed by length-prefixed name.	Registers are most significant first
	const char*		name() const { return (const char*)(reg + count); }
};
struct ATTRMANYREGSYM : SYMTYPE { // S_MANMANYREG
	type_t			typind;	// Type index or metadata token
	lvar_attr		attr;	// local var attributes
	uint8			count;	// count of number of registers
	uint8			reg[1];	// count register enumerates followed by length-prefixed name.	Registers are most significant first
	const char*		name() const { return (const char*)(reg + count); }
};
struct ATTRMANYREGSYM2 : SYMTYPE { // S_MANMANYREG2 | S_ATTR_MANYREG
	type_t			typind;	// Type index or metadata token
	lvar_attr		attr;	// local var attributes
	uint16			count;	// count of number of registers
	uint16			reg[1];	// count register enumerates followed by length-prefixed name.	Registers are most significant first
	const char*		name() const { return (const char*)(reg + count); }
};
struct CONSTSYM_16t : SYMTYPE { // S_CONSTANT_16t
	type16_t		typind;	// Type index (containing enum if enumerate)
	Value			value;	// numeric leaf containing value
	const char	*name() const { return value.after(); }
};
struct CONSTSYM : SYMTYPE { // S_CONSTANT or S_MANCONSTANT
	type_t			typind;	// Type index (containing enum if enumerate) or metadata token
	Value			value;	// numeric leaf containing value
	const char *name() const { return value.after(); }
};
struct UDTSYM_16t : SYMTYPE { // S_UDT_16t | S_COBOLUDT_16t
	type16_t		typind;	// Type index
	embedded_string	name;	// Length-prefixed name
};
struct UDTSYM : SYMTYPE { // S_UDT | S_COBOLUDT
	type_t			typind;	// Type index
	embedded_string	name;	// Length-prefixed name
};
struct MANTYPREF : SYMTYPE { // S_MANTYPREF
	type_t			typind;	// Type index
};
struct SEARCHSYM : SYMTYPE { // S_SSEARCH
	segmented32		addr;		// addr of the procedure
};
struct ENDSYM : SYMTYPE { // S_END, S_INLINESITE_END, S_PROC_ID_END (AJS)
	uint32			offset;
};

struct CFLAGSYM : SYMTYPE { // S_COMPILE
	enum MODEL {
		Near	= 0,
		Far		= 1,
		Huge	= 2
	};
	enum FPKG {
		NDP		= 0,
		EMU		= 1,
		ALT		= 2
	};
	uint8		machine;	// target processor
	struct {
		uint8		language	: 8;	// language index
		uint8		pcode		: 1;	// true if pcode present
		uint8		floatprec	: 2;	// floating precision
		uint8		floatpkg	: 2;	// float package
		uint8		ambdata		: 3;	// ambient data model
		uint8		ambcode		: 3;	// ambient code model
		uint8		mode32		: 1;	// true if compiled 32 bit mode
		uint8		pad			: 4;	// reserved
	} flags;
	uint8		ver[1];	// Length-prefixed compiler version string
};
struct COMPILESYM : SYMTYPE { // S_COMPILE2
	struct {
		uint32		iLanguage		: 8;	// language index
		uint32		fEC				: 1;	// compiled for E/C
		uint32		fNoDbgInfo		: 1;	// not compiled with debug info
		uint32		fLTCG			: 1;	// compiled with LTCG
		uint32		fNoDataAlign	: 1;	// compiled with -Bzalign
		uint32		fManagedPresent : 1;	// managed code/data present
		uint32		fSecurityChecks : 1;	// compiled with /GS
		uint32		fHotPatch		: 1;	// compiled with /hotpatch
		uint32		fCVTCIL			: 1;	// converted with CVTCIL
		uint32		fMSILModule		: 1;	// MSIL netmodule
		uint32		pad				: 15;	// reserved, must be 0
	} flags;
	uint16		machine;	// target processor
	uint16		verFEMajor;	// front end major version #
	uint16		verFEMinor;	// front end minor version #
	uint16		verFEBuild;	// front end build version #
	uint16		verMajor;	// back end major version #
	uint16		verMinor;	// back end minor version #
	uint16		verBuild;	// back end build version #
	uint8		verSt[1];	// Length-prefixed compiler version string, followed by an optional block of zero terminated strings terminated with a double zero
};
struct COMPILESYM3 : SYMTYPE { // S_COMPILE3
	struct {
		uint32		iLanguage		: 8;	// language index
		uint32		fEC				: 1;	// compiled for E/C
		uint32		fNoDbgInfo		: 1;	// not compiled with debug info
		uint32		fLTCG			: 1;	// compiled with LTCG
		uint32		fNoDataAlign	: 1;	// compiled with -Bzalign
		uint32		fManagedPresent : 1;	// managed code/data present
		uint32		fSecurityChecks : 1;	// compiled with /GS
		uint32		fHotPatch		: 1;	// compiled with /hotpatch
		uint32		fCVTCIL			: 1;	// converted with CVTCIL
		uint32		fMSILModule		: 1;	// MSIL netmodule
		uint32		fSdl			: 1;	// compiled with /sdl
		uint32		fPGO			: 1;	// compiled with /ltcg:pgo or pgu
		uint32		fExp			: 1;	// .exp module
		uint32		pad				: 12;	// reserved, must be 0
	} flags;
	uint16		machine;	// target processor
	uint16		verFEMajor;	// front end major version #
	uint16		verFEMinor;	// front end minor version #
	uint16		verFEBuild;	// front end build version #
	uint16		verFEQFE;	// front end QFE version #
	uint16		verMajor;	// back end major version #
	uint16		verMinor;	// back end minor version #
	uint16		verBuild;	// back end build version #
	uint16		verQFE;		// back end QFE version #
	char		verSz[1];	// Zero terminated compiler version string
};
struct ENVBLOCKSYM : SYMTYPE { // S_ENVBLOCK
	struct {
		uint8		rev : 1;	// reserved
		uint8		pad : 7;	// reserved, must be 0
	} flags;
	embedded_multi_string<char>	rgsz;	// Sequence of zero-terminated strings
};
struct OBJNAMESYM : SYMTYPE { // S_OBJNAME
	uint32			signature;	// signature
	embedded_string	name;
};
struct ENDARGSYM : SYMTYPE { // S_ENDARG
};

struct RETURNSYM : SYMTYPE { // S_RETURN
	// enum describing function data return method
	enum STYLE {
		GENERIC_VOID	= 0,	// void return type
		GENERIC_REG 	= 1,	// return data is in registers
		GENERIC_ICAN	= 2,	// indirect caller allocated near
		GENERIC_ICAF	= 3,	// indirect caller allocated far
		GENERIC_IRAN	= 4,	// indirect returnee allocated near
		GENERIC_IRAF	= 5,	// indirect returnee allocated far
		GENERIC_UNUSED			// first unused
	};
	struct {
		uint16		cstyle	: 1;	// true push varargs right to left
		uint16		rsclean	: 1;	// true if returnee stack cleanup
		uint16		unused	: 14;	// unused
	}				flags;
	uint8			style;	// STYLE return style followed by return method data
};
struct ENTRYTHISSYM : SYMTYPE { // S_ENTRYTHIS
	uint8			thissym;	// symbol describing this pointer on entry
};

//	symbol types for 16:16 memory model
struct BPRELSYM16 : SYMTYPE { // S_BPREL16
	int16			off;		// BP-relative offset
	type16_t		typind;		// Type index
	embedded_string	name;		// Length-prefixed name
};
struct DATASYM16 : SYMTYPE { // S_LDATA or S_GDATA
	segmented16		addr;		// addr of symbol
	type16_t		typind;		// Type index
	embedded_string	name;		// Length-prefixed name
};
typedef DATASYM16 PUBSYM16;

//	generic block definition symbols; similar to the equivalent 16:16 or 16:32 symbols but only define the length, type and linkage fields
struct BLOCKSYM : SYMTYPE {
	uint32		pParent;	// pointer to the parent
	uint32		pEnd;		// pointer to this blocks end
};

struct PROCSYM : BLOCKSYM {
	uint32		pNext;		// pointer to next symbol
	auto		skip_children(const next_iterator<const SYMTYPE> &start)	const	{ return start.at_offset(pEnd); }
	auto		children(const range<next_iterator<const SYMTYPE>> &syms)	const	{ return make_next_range(next(), pEnd ? syms.begin().at_offset(pEnd) : &*syms.end()); }
};

struct PROCSYM16 : PROCSYM { // S_GPROC16 or S_LPROC16
	uint16			len;		// Proc length
	uint16			DbgStart;	// Debug start offset
	uint16			DbgEnd;		// Debug end offset
	segmented16		addr;		// addr of symbol
	type16_t		typind;		// Type index
	PROCFLAGS		flags;		// Proc flags
	embedded_string	name;		// Length-prefixed name
	bool		encompasses(uint32 a)	const { return a >= addr.off && a - addr.off < len; }
};
struct THUNKSYM16 : PROCSYM { // S_THUNK
	segmented16		addr;		// addr of symbol
	uint16			len;		// length of thunk
	uint8			ord;		// THUNK_ORDINAL specifying type of thunk
	embedded_string	name;		// name of thunk
	uint8			variant[];	// variant portion of thunk
	bool		encompasses(uint32 a)	const { return a >= addr.off && a - addr.off < len; }
};
struct LABELSYM16 : SYMTYPE { // S_LABEL16
	segmented16		addr;		// addr of symbol
	PROCFLAGS		flags;		// flags
	embedded_string	name;		// Length-prefixed name
};
struct BLOCKSYM16 : BLOCKSYM { // S_BLOCK16
	uint16			len;		// Block length
	segmented16		addr;		// addr of symbol
	embedded_string	name;		// Length-prefixed name
};
struct WITHSYM16 : BLOCKSYM { // S_WITH16
	uint16			len;		// Block length
	segmented16		addr;		// addr of symbol
	uint8			expr[1];	// Length-prefixed expression
};
enum CEXM_MODEL_e {
	CEXM_MDL_table 			= 0x00, // not executable
	CEXM_MDL_jumptable 		= 0x01, // Compiler generated jump table
	CEXM_MDL_datapad 		= 0x02, // Data padding for alignment
	CEXM_MDL_native 		= 0x20, // native (actually not-pcode)
	CEXM_MDL_cobol 			= 0x21, // cobol
	CEXM_MDL_codepad 		= 0x22, // Code padding for alignment
	CEXM_MDL_code 			= 0x23, // code
	CEXM_MDL_sql 			= 0x30, // sql
	CEXM_MDL_pcode 			= 0x40, // pcode
	CEXM_MDL_pcode32Mac		= 0x41, // macintosh 32 bit pcode
	CEXM_MDL_pcode32MacNep	= 0x42, // macintosh 32 bit pcode native entry point
	CEXM_MDL_javaInt 		= 0x50,
	CEXM_MDL_unknown 		= 0xff
};

// use the correct enumerate name
#define CEXM_MDL_SQL CEXM_MDL_sql
enum COBOL_e {
	COBOL_dontstop,
	COBOL_pfm,
	COBOL_false,
	COBOL_extcall
};
struct CEXMSYM16 : SYMTYPE { // S_CEXMODEL16
	segmented16		addr;		// addr of symbol
	uint16			model;		// execution model
	union {
		struct {
			uint16 pcdtable;	// offset to pcode function table
			uint16 pcdspi;	// offset to segment pcode information
		} pcode;
		struct {
			uint16	subtype;	// see COBOL_e above
			uint16	flag;
		} cobol;
	};
};
struct VPATHSYM16 : SYMTYPE { // S_VFTPATH16
	segmented16		addr;	// addr of virtual function table
	type16_t		root;	// type index of the root of path
	type16_t		path;	// type index of the path record
};
struct REGREL16 : SYMTYPE { // S_REGREL16
	uint16			off;	// offset of symbol
	uint16			reg;	// register index
	type16_t		typind;	// Type index
	embedded_string	name;	// Length-prefixed name
};
struct BPRELSYM32_16t : SYMTYPE { // S_BPREL32_16t
	int32			off;	// BP-relative offset
	type16_t		typind;	// Type index
	embedded_string	name;	// Length-prefixed name
};
struct BPRELSYM32 : SYMTYPE { // S_BPREL32
	int32			off;	// BP-relative offset
	type_t			typind;	// Type index or Metadata token
	embedded_string	name;	// Length-prefixed name
};
struct FRAMERELSYM : SYMTYPE { // S_MANFRAMEREL | S_ATTR_FRAMEREL
	int32			off;	// Frame relative offset
	type_t			typind;	// Type index or Metadata token
	lvar_attr		attr;	// local var attributes
	embedded_string	name;	// Length-prefixed name
};
typedef FRAMERELSYM ATTRFRAMERELSYM;
struct SLOTSYM32 : SYMTYPE { // S_LOCALSLOT or S_PARAMSLOT
	uint32			iSlot;		// slot index
	type_t			typind;		// Type index or Metadata token
	embedded_string	name;
};
struct ATTRSLOTSYM : SYMTYPE { // S_MANSLOT
	uint32			iSlot;		// slot index
	type_t			typind;		// Type index or Metadata token
	lvar_attr		attr;		// local var attributes
	embedded_string	name;
};
struct ANNOTATIONSYM : SYMTYPE { // S_ANNOTATION
	segmented32		addr;
	uint16			csz;		// Count of zero terminated annotation strings
	uint8			rgsz[1];	// Sequence of zero terminated annotation strings
};
struct DATASYM32_16t : SYMTYPE { // S_LDATA32_16t, S_GDATA32_16t or S_PUB32_16t
	segmented32		addr;
	type16_t		typind;		// Type index
	embedded_string	name;
};
typedef DATASYM32_16t PUBSYM32_16t;
struct DATASYM32 : SYMTYPE { // S_LDATA32, S_GDATA32, S_LMANDATA, S_GMANDATA
	type_t			typind;		// Type index, or Metadata token if a managed symbol
	segmented32		addr;
	embedded_string	name;
};
struct DATASYMHLSL : SYMTYPE { // S_GDATA_HLSL, S_LDATA_HLSL
	type_t			typind;		// Type index
	uint16			regType;	// register type from HLSLREG_e
	uint16			dataslot;	// Leaf data (cbuffer, groupshared, etc.) slot
	uint16			dataoff;	// Leaf data byte offset start
	uint16			texslot;	// Texture slot start
	uint16			sampslot;	// Sampler slot start
	uint16			uavslot;	// UAV slot start
	embedded_string	name;
};
struct DATASYMHLSL32 : SYMTYPE { // S_GDATA_HLSL32, S_LDATA_HLSL32
	type_t			typind;		// Type index
	uint32			dataslot;	// Leaf data (cbuffer, groupshared, etc.) slot
	uint32			dataoff;	// Leaf data byte offset start
	uint32			texslot;	// Texture slot start
	uint32			sampslot;	// Sampler slot start
	uint32			uavslot;	// UAV slot start
	uint16			regType;	// register type from HLSLREG_e
	embedded_string	name;
};
struct DATASYMHLSL32_EX : SYMTYPE { // S_GDATA_HLSL32_EX, S_LDATA_HLSL32_EX
	type_t			typind;		// Type index
	uint32			regID;		// Register index
	uint32			dataoff;	// Leaf data byte offset start
	uint32			bindSpace;	// Binding space
	uint32			bindSlot;	// Lower bound in binding space
	uint16			regType;	// register type from HLSLREG_e
	embedded_string	name;
};
struct PUBSYM32 : SYMTYPE { // S_PUB32
	union {
		uint32 u;
		struct {
			uint32 code		: 1;	// set if public symbol refers to a code address
			uint32 function	: 1;	// set if public symbol is a function
			uint32 managed	: 1;	// set if managed code (native or IL)
			uint32 MSIL		: 1;	// set if managed IL code
			uint32 __unused	: 28;	// must be zero
		};
	}				flags;
	segmented32		addr;
	embedded_string	name;
};
struct PROCSYM32_16t : PROCSYM { // S_GPROC32_16t or S_LPROC32_16t
	uint32			len;		// Proc length
	uint32			DbgStart;	// Debug start offset
	uint32			DbgEnd;		// Debug end offset
	segmented32		addr;
	type16_t		typind;		// Type index
	PROCFLAGS		flags;		// Proc flags
	embedded_string	name;
	bool		encompasses(uint32 a)	const { return a >= addr.off && a - addr.off < len; }
};
struct PROCSYM32 : PROCSYM { // S_GPROC32, S_LPROC32, S_GPROC32_ID, S_LPROC32_ID, S_LPROC32_DPC or S_LPROC32_DPC_ID
	uint32			len;		// Proc length
	uint32			DbgStart;	// Debug start offset
	uint32			DbgEnd;		// Debug end offset
	type_t			typind;		// Type index or ID
	segmented32		addr;
	PROCFLAGS		flags;		// Proc flags
	embedded_string	name;
	bool		encompasses(uint32 a)	const { return a >= addr.off && a - addr.off < len; }
};
struct MANPROCSYM : PROCSYM { // S_GMANPROC, S_LMANPROC, S_GMANPROCIA64 or S_LMANPROCIA64
	uint32			len;		// Proc length
	uint32			DbgStart;	// Debug start offset
	uint32			DbgEnd;		// Debug end offset
	token_t			token;		// COM+ metadata token for method
	segmented32		addr;
	PROCFLAGS		flags;		// Proc flags
	uint16			retReg;		// Register return value is in (may not be used for all archs)
	embedded_string	name;		// optional name field
	bool		encompasses(uint32 a)	const { return a >= addr.off && a - addr.off < len; }
};
struct MANPROCSYMMIPS : PROCSYM { // S_GMANPROCMIPS or S_LMANPROCMIPS
	uint32			len;		// Proc length
	uint32			DbgStart;	// Debug start offset
	uint32			DbgEnd;		// Debug end offset
	uint32			regSave;	// int register save mask
	uint32			fpSave;		// fp register save mask
	uint32			intOff;		// int register save offset
	uint32			fpOff;		// fp register save offset
	token_t			token;		// COM+ token type
	segmented32		addr;
	uint8			retReg;		// Register return value is in
	uint8			frameReg;	// Frame pointer register
	embedded_string	name;		// optional name field
	bool		encompasses(uint32 a)	const { return a >= addr.off && a - addr.off < len; }
};
struct THUNKSYM32 : PROCSYM { // S_THUNK32
	segmented32		addr;
	uint16			len;		// length of thunk
	uint8			ord;		// THUNK_ORDINAL specifying type of thunk
	embedded_string	name;
	uint8			variant[];	// variant portion of thunk
	bool		encompasses(uint32 a)	const { return a >= addr.off && a - addr.off < len; }
};
enum TRAMP_e { // Trampoline subtype
	trampIncremental,	// incremental thunks
	trampBranchIsland,	// Branch island thunks
};
struct TRAMPOLINESYM : SYMTYPE { // S_TRAMPOLINE
	uint16			trampType;	// trampoline sym subtype
	uint16			cbThunk;	// size of the thunk
	uint32			offThunk;	// offset of the thunk
	uint32			offTarget;	// offset of the target of the thunk
	uint16			sectThunk;	// section index of the thunk
	uint16			sectTarget;	// section index of the target of the thunk
};
struct LABELSYM32 : SYMTYPE { // S_LABEL32
	segmented32		addr;
	PROCFLAGS		flags;
	embedded_string	name;
};
struct BLOCKSYM32 : BLOCKSYM { // S_BLOCK32
	uint32			len;		// Block length
	segmented32		addr;
	embedded_string	name;
	bool		after(uint32 a)			const { return addr.off > a; }
	bool		encompasses(uint32 a)	const { return a >= addr.off && a - addr.off < len; }
};
struct WITHSYM32 : BLOCKSYM { // S_WITH32
	uint32			len;		// Block length
	segmented32		addr;
	uint8			expr[1];	// Length-prefixed expression string
	bool		encompasses(uint32 a)	const { return a >= addr.off && a - addr.off < len; }
};
struct CEXMSYM32 : SYMTYPE { // S_CEXMODEL32
	segmented32		addr;
	uint16			model;		// execution model
	union {
		struct {
			uint32 pcdtable;	// offset to pcode function table
			uint32 pcdspi;	// offset to segment pcode information
		} pcode;
		struct {
			uint16	subtype;	// see COBOL_e above
			uint16	flag;
		} cobol;
		segmented32	pcode32Mac;	// to function table
	};
};
struct VPATHSYM32_16t : SYMTYPE { // S_VFTABLE32_16t
	segmented32		addr;	// addr of virtual function table
	type16_t		root;	// type index of the root of path
	type16_t		path;	// type index of the path record
};
struct VPATHSYM32 : SYMTYPE { // S_VFTABLE32
	type_t			root;	// type index of the root of path
	type_t			path;	// type index of the path record
	segmented32		addr;	// addr of virtual function table
};
struct REGREL32_16t : SYMTYPE { // S_REGREL32_16t
	uint32			off;	// offset of symbol
	uint16			reg;	// register index for symbol
	type16_t		typind;	// Type index
	embedded_string	name;
};
struct REGREL32 : SYMTYPE { // S_REGREL32
	uint32			off;	// offset of symbol
	type_t			typind;	// Type index or metadata token
	uint16			reg;	// register index for symbol
	embedded_string	name;
};
struct ATTRREGREL : SYMTYPE { // S_MANREGREL | S_ATTR_REGREL
	uint32			off;	// offset of symbol
	type_t			typind;	// Type index or metadata token
	uint16			reg;	// register index for symbol
	lvar_attr		attr;	// local var attributes
	embedded_string	name;
};
typedef ATTRREGREL	ATTRREGRELSYM;
struct THREADSYM32_16t : SYMTYPE { // S_LTHREAD32_16t | S_GTHREAD32_16t
	segmented32		addr;	// offset into thread storage
	type16_t		typind;	// type index
	embedded_string	name;
};
struct THREADSYM32 : SYMTYPE { // S_LTHREAD32 | S_GTHREAD32
	type_t			typind;	// type index
	segmented32		addr;	// offset into thread storage
	embedded_string	name;
};
struct SLINK32 : SYMTYPE { // S_SLINK32
	uint32			framesize;	// frame size of parent procedure
	int32			off;		// signed offset where the static link was saved relative to the value of reg
	uint16			reg;
};
struct PROCSYMMIPS_16t : PROCSYM { // S_GPROCMIPS_16t or S_LPROCMIPS_16t
	uint32			len;		// Proc length
	uint32			DbgStart;	// Debug start offset
	uint32			DbgEnd;		// Debug end offset
	uint32			regSave;	// int register save mask
	uint32			fpSave;		// fp register save mask
	uint32			intOff;		// int register save offset
	uint32			fpOff;		// fp register save offset
	segmented32		addr;		// Symbol addr
	type16_t			typind;		// Type index
	uint8			retReg;		// Register return value is in
	uint8			frameReg;	// Frame pointer register
	embedded_string	name;
	bool		encompasses(uint32 a)	const { return a >= addr.off && a - addr.off < len; }
};
struct PROCSYMMIPS : PROCSYM { // S_GPROCMIPS or S_LPROCMIPS
	uint32			len;		// Proc length
	uint32			DbgStart;	// Debug start offset
	uint32			DbgEnd;		// Debug end offset
	uint32			regSave;	// int register save mask
	uint32			fpSave;		// fp register save mask
	uint32			intOff;		// int register save offset
	uint32			fpOff;		// fp register save offset
	type_t			typind;		// Type index
	segmented32		addr;		// Symbol addr
	uint8			retReg;		// Register return value is in
	uint8			frameReg;	// Frame pointer register
	embedded_string	name;
	bool		encompasses(uint32 a)	const { return a >= addr.off && a - addr.off < len; }
};
struct PROCSYMIA64 : PROCSYM { // S_GPROCIA64 or S_LPROCIA64
	uint32			len;		// Proc length
	uint32			DbgStart;	// Debug start offset
	uint32			DbgEnd;		// Debug end offset
	type_t			typind;		// Type index
	segmented32		addr;		// Symbol addr
	uint16			retReg;		// Register return value is in
	PROCFLAGS		flags;		// Proc flags
	embedded_string	name;
	bool		encompasses(uint32 a)	const { return a >= addr.off && a - addr.off < len; }
};
struct REFSYM : SYMTYPE { // S_PROCREF_ST, S_DATAREF_ST, or S_LPROCREF_ST
	uint32			sumName;	// SUC of the name
	uint32			ibSym;		// Offset of actual symbol in $$Symbols
	uint16			imod;		// Module containing the actual symbol
	uint16			usFill;		// align this record
	embedded_string	name;
};
struct REFSYM2 : SYMTYPE { // S_PROCREF, S_DATAREF, or S_LPROCREF
	uint32			sumName;	// SUC of the name
	uint32			ibSym;		// Offset of actual symbol in $$Symbols
	uint16			imod;		// Module containing the actual symbol
	embedded_string	name;		// hidden name made a first class member
};
struct ALIGNSYM : SYMTYPE { // S_ALIGN
	embedded_string	name;	// AJS added this!
};
struct OEMSYMBOL : SYMTYPE { // S_OEM
	uint8			idOem[16];	// an oem ID (GUID)
	type_t			typind;		// Type index
	uint32			rgl[];		// user data, force 4-byte alignment
};

struct WITHSYM : BLOCKSYM { // S_WITH16
};
struct FRAMEPROCSYM : SYMTYPE { // S_FRAMEPROC
	uint32		cbFrame;	// count of bytes of total frame of procedure
	uint32		cbPad;		// count of bytes of padding in the frame
	uint32		offPad;		// offset (relative to frame poniter) to where padding starts
	uint32		cbSaveRegs;	// count of bytes of callee save registers
	uint32		offExHdlr;	// offset of exception handler
	uint16		sectExHdlr;	// section id of exception handler
	struct {
		uint32		fHasAlloca				: 1;	// function uses _alloca()
		uint32		fHasSetJmp				: 1;	// function uses setjmp()
		uint32		fHasLongJmp				: 1;	// function uses longjmp()
		uint32		fHasInlAsm				: 1;	// function uses inline asm
		uint32		fHasEH					: 1;	// function has EH states
		uint32		fInlSpec				: 1;	// function was speced as inline
		uint32		fHasSEH					: 1;	// function has SEH
		uint32		fNaked					: 1;	// function is __declspec(naked)
		uint32		fSecurityChecks			: 1;	// function has buffer security check introduced by /GS
		uint32		fAsyncEH				: 1;	// function compiled with /EHa
		uint32		fGSNoStackOrdering		: 1;	// function has /GS buffer checks, but stack ordering couldn't be done
		uint32		fWasInlined				: 1;	// function was inlined within another function
		uint32		fGSCheck				: 1;	// function is __declspec(strict_gs_check)
		uint32		fSafeBuffers			: 1;	// function is __declspec(safebuffers)
		uint32		encodedLocalBasePointer : 2;	// record function's local pointer explicitly
		uint32		encodedParamBasePointer : 2;	// record function's parameter pointer explicitly
		uint32		fPogoOn					: 1;	// function was compiled with PGO/PGU
		uint32		fValidCounts			: 1;	// Do we have valid Pogo counts?
		uint32		fOptSpeed				: 1;	// Did we optimize for speed?
		uint32		fGuardCF				: 1;	// function contains CFG checks (and no write checks)
		uint32		fGuardCFW				: 1;	// function contains CFW checks and/or instrumentation
		uint32		pad						: 9;	// must be zero
	} flags;

	static uint16 ExpandEncoded(unsigned machine, unsigned encoded) {
		static const uint16 X86[] = { REG_NONE, CV_ALLREG_VFRAME, CV_REG_EBP, CV_REG_EBX };
		static const uint16 X64[] = { REG_NONE, CV_AMD64_RSP, CV_AMD64_RBP, CV_AMD64_R13 };
		static const uint16 Arm[] = { REG_NONE, CV_ARM_SP, CV_ARM_R7, REG_NONE };
		if (encoded >= 4)
			return REG_NONE;
		switch (machine) {
			case CV_CFL_8080:
			case CV_CFL_8086:
			case CV_CFL_80286:
			case CV_CFL_80386:
			case CV_CFL_80486:
			case CV_CFL_PENTIUM:
			case CV_CFL_PENTIUMII:
			case CV_CFL_PENTIUMIII:	return X86[encoded];
			case CV_CFL_AMD64:		return X64[encoded];
			case CV_CFL_ARMNT:		return Arm[encoded];
			default:				return REG_NONE;
		}
	}

	uint16	BasePointer(unsigned machine)	const { return ExpandEncoded(machine, flags.encodedLocalBasePointer); }
	uint16	ParamsPointer(unsigned machine)	const { return ExpandEncoded(machine, flags.encodedParamBasePointer); }
};

struct UNAMESPACE : SYMTYPE { // S_UNAMESPACE
	embedded_string	name;			// name
};
struct SEPCODESYM : BLOCKSYM { // S_SEPCODE
	uint32			length;				// count of bytes of this block
	struct {
		uint32 fIsLexicalScope	: 1;	// S_SEPCODE doubles as lexical scope
		uint32 fReturnsToParent : 1;	// code frag returns to parent
		uint32 pad				: 30;	// must be zero
	}				scf;				// flags
	uint32			off;				// sect:off of the separated code
	uint32			offParent;			// sectParent:offParent of the enclosing scope
	uint16			sect;				//	(proc, block, or sepcode)
	uint16			sectParent;
};
struct BUILDINFOSYM : SYMTYPE { // S_BUILDINFO
	ItemId		id;					// ItemId of Build Info
};
struct INLINESITESYM : BLOCKSYM { // S_INLINESITE
	ItemId			inlinee;			// ItemId of inlinee
	uint8			binaryAnnotations[];// an array of compressed binary annotations
	LineNumberParser	lines(uint32 offset = 0) const;
};

struct INLINESITESYM2 : BLOCKSYM { // S_INLINESITE2
	ItemId			inlinee;			// ItemId of inlinee
	uint32			invocations;		// entry count
	uint8			binaryAnnotations[];// an array of compressed binary annotations
	LineNumberParser	lines(uint32 offset = 0) const;
};

// Defines a local and its live range, how to evaluate
// S_DEFRANGE modifies previous local S_LOCAL, it has to consecutive
struct LOCALSYM : SYMTYPE { // S_LOCAL
	type_t			typind;			// type index
	LVARFLAGS		flags;			// local var flags
	embedded_string	name;
};
struct FILESTATICSYM : SYMTYPE { // S_FILESTATIC
	type_t			typind;			// type index
	uint32			modOffset;		// index of mod filename in stringtable
	LVARFLAGS		flags;			// local var flags
	embedded_string	name;
};
struct DEFRANGESYM : SYMTYPE { // S_DEFRANGE
	uint32			program;		// DIA program to evaluate the value of the symbol
	LVAR_ADDR_RANGE	_range;			// Range of addresses where this program is valid
	LVAR_ADDR_RANGE2	range()		const	{ return LVAR_ADDR_RANGE2(&_range, next()); }
};
struct DEFRANGESYMSUBFIELD : SYMTYPE { // S_DEFRANGE_SUBFIELD
	uint32			program;		// DIA program to evaluate the value of the symbol
	uint32			offParent;		// Offset in parent variable
	LVAR_ADDR_RANGE	_range;			// Range of addresses where this program is valid
	LVAR_ADDR_RANGE2	range()		const	{ return LVAR_ADDR_RANGE2(&_range, next()); }
};
struct RANGEATTR {
	uint16			maybe : 1;		// May have no user name on one of control flow path
	uint16			padding : 15;	// Padding for future use
};
struct DEFRANGESYMREGISTER : SYMTYPE { // S_DEFRANGE_REGISTER
	uint16			reg;				// Register to hold the value of the symbol
	RANGEATTR		attr;				// Attribute of the register range
	LVAR_ADDR_RANGE	_range;				// Range of addresses where this program is valid
	LVAR_ADDR_RANGE2	range()		const	{ return LVAR_ADDR_RANGE2(&_range, next()); }
};
struct DEFRANGESYMFRAMEPOINTERREL : SYMTYPE { // S_DEFRANGE_FRAMEPOINTER_REL
	int32			offFramePointer;	// offset to frame pointer
	LVAR_ADDR_RANGE	_range;				// Range of addresses where this program is valid
	LVAR_ADDR_RANGE2	range()		const	{ return LVAR_ADDR_RANGE2(&_range, next()); }
};
struct DEFRANGESYMFRAMEPOINTERREL_FULL_SCOPE : SYMTYPE { // S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE
	int32			offFramePointer;// offset to frame pointer
};
#define OFFSET_PARENT_LENGTH_LIMIT 12
// Note DEFRANGESYMREGISTERREL and DEFRANGESYMSUBFIELDREGISTER had same layout.
struct DEFRANGESYMSUBFIELDREGISTER : SYMTYPE { // S_DEFRANGE_SUBFIELD_REGISTER
	uint16			reg;					// Register to hold the value of the symbol
	RANGEATTR		attr;					// Attribute of the register range
	uint32			offParent	: OFFSET_PARENT_LENGTH_LIMIT;		// Offset in parent variable
	uint32			padding		: 20;		// Padding for future use
	LVAR_ADDR_RANGE	_range;					// Range of addresses where this program is valid
	LVAR_ADDR_RANGE2	range()		const	{ return LVAR_ADDR_RANGE2(&_range, next()); }
};

// Note DEFRANGESYMREGISTERREL and DEFRANGESYMSUBFIELDREGISTER had same layout
// Used when /GS Copy parameter as local variable or other variable don't cover by FRAMERELATIVE
struct DEFRANGESYMREGISTERREL : SYMTYPE { // S_DEFRANGE_REGISTER_REL
	uint16			baseReg;				// Register to hold the base pointer of the symbol
	uint16			spilledUdtMember: 1;	// Spilled member for s.i
	uint16			padding 		: 3;	// Padding for future use
	uint16			offsetParent 	: OFFSET_PARENT_LENGTH_LIMIT;	// Offset in parent variable
	int32			offBasePointer;			// offset to register
	LVAR_ADDR_RANGE	_range;					// Range of addresses where this program is valid
	LVAR_ADDR_RANGE2	range()		const	{ return LVAR_ADDR_RANGE2(&_range, next()); }
};
struct DEFRANGESYMHLSL : SYMTYPE { // S_DEFRANGE_HLSL or S_DEFRANGE_DPC_PTR_TAG
	uint16			regType;				// register type from HLSLREG_e
	uint16			regIndices 		: 2;	// 0, 1 or 2, dimensionality of register space
	uint16			spilledUdtMember: 1;	// this is a spilled member
	uint16			memorySpace 	: 4;	// memory space from HLSLMemorySpace_e
	uint16			padding 		: 9;	// for future use
	uint16			offsetParent;			// Offset in parent variable
	uint16			sizeInParent;			// Size of enregistered portion
	LVAR_ADDR_RANGE	_range;					// Range of addresses where this program is valid

	range<const uint32*> offsets()	const	{ return make_range_n((const uint32*)next() - regIndices, regIndices); }
//	const uint32		*offsets()	const	{ return (uint32*)next() - regIndices; }
	LVAR_ADDR_RANGE2	range()		const	{ return LVAR_ADDR_RANGE2(&_range, (uint32*)next() - regIndices); }
};

struct FUNCTIONLIST : SYMTYPE { // S_CALLERS, S_CALLEES, S_INLINEES
	uint32			count;			// Number of functions
	type_t			funcs[];		// List of functions, dim == count uint32	invocations[]; Followed by a parallel array of invocation counts. Counts > reclen are assumed to be zero
	auto	entries() const { return make_range_n(funcs, count); }
};
struct POGOINFO : SYMTYPE { // S_POGODATA
	uint32			invocations;	// Number of times function was called
	int64			dynCount;		// Dynamic instruction count
	uint32			numInstrs;		// Static instruction count
	uint32			staInstLive;	// Final static instruction count (post inlining)
};
struct ARMSWITCHTABLE : SYMTYPE { // S_ARMSWITCHTABLE
	enum SwitchType {
		SWT_INT1		= 0,
		SWT_UINT1		= 1,
		SWT_INT2		= 2,
		SWT_UINT2		= 3,
		SWT_INT4		= 4,
		SWT_UINT4		= 5,
		SWT_POINTER		= 6,
		SWT_UINT1SHL1	= 7,
		SWT_UINT2SHL1	= 8,
		SWT_INT1SHL1	= 9,
		SWT_INT2SHL1	= 10,
		SWT_TBB			= SWT_UINT1SHL1,
		SWT_TBH			= SWT_UINT2SHL1,
	};
	uint32			offsetBase;		// Section-relative offset to the base for switch offsets
	uint16			sectBase;		// Section index of the base for switch offsets
	uint16			switchType;		// type of each entry
	uint32			offsetBranch;	// Section-relative offset to the table branch instruction
	uint32			offsetTable;	// Section-relative offset to the start of the table
	uint16			sectBranch;		// Section index of the table branch instruction
	uint16			sectTable;		// Section index of the table
	uint32			cEntries;		// number of switch table entries
};
struct MODTYPEREF : SYMTYPE { // S_MOD_TYPEREF
	uint32			fNone : 1;		// module doesn't reference any type
	uint32			fRefTMPCT : 1;	// reference /Z7 PCH types
	uint32			fOwnTMPCT : 1;	// module contains /Z7 PCH types
	uint32			fOwnTMR : 1;	// module contains type info (/Z7)
	uint32			fOwnTM : 1;		// module contains type info (/Zi or /ZI)
	uint32			fRefTM : 1;		// module references type info owned by other module
	uint32			reserved : 9;
	uint16			word0;			// these two words contain SN or module index depending
	uint16			word1;			// on above flags
};
struct SECTIONSYM : SYMTYPE { // S_SECTION
	uint16			isec;			// Section number
	uint8			align;			// Alignment of this section (power of 2)
	uint8			bReserved;		// Reserved.	Must be zero
	uint32			rva;
	uint32			cb;
	uint32			characteristics;
	embedded_string	name;
};
struct COFFGROUPSYM : SYMTYPE { // S_COFFGROUP
	uint32			cb;
	uint32			characteristics;
	segmented32		addr;		// Symbol addr
	embedded_string	name;
};
struct EXPORTSYM : SYMTYPE { // S_EXPORT
	uint16			ordinal;
	uint16			fConstant : 1;	// CONSTANT
	uint16			fData : 1;		// DATA
	uint16			fPrivate : 1;	// PRIVATE
	uint16			fNoName : 1;	// NONAME
	uint16			fOrdinal : 1;	// Ordinal was explicitly assigned
	uint16			fForwarder : 1;	// This is a forwarder
	uint16			reserved : 10;	// Reserved. Must be zero
	embedded_string	name;
};

// Symbol for describing indirect calls when they are using a function pointer cast on some other type or temporary
// Typical content will be an LF_POINTER to an LF_PROCEDURE type record that should mimic an actual variable with the function pointer type in question
// Since the compiler can sometimes tail-merge a function call through a function pointer, there may be more than one S_CALLSITEINFO record at an address
// This is similar to what you could do in your own code by:
//
//	pfn = expr ? &function1 : &function2;
//	(*pfn)(arg list);

struct CALLSITEINFO : SYMTYPE { // S_CALLSITEINFO
	int32		off;			// offset of call site
	uint16		sect;			// section index of call site
	uint16		__reserved_0;	// alignment padding field, must be zero
	type_t		typind;			// type index describing function signature
};
struct HEAPALLOCSITE : SYMTYPE { // S_HEAPALLOCSITE
	int32		off;			// offset of call site
	uint16		sect;			// section index of call site
	uint16		cbInstr;		// length of heap allocation call instruction
	type_t		typind;			// type index describing function signature
};

// Symbol for describing security cookie's position and type (raw, xor'd with esp, xor'd with ebp)
struct FRAMECOOKIE : SYMTYPE { // S_FRAMECOOKIE
	enum Type {
		COPY = 0,
		XOR_SP,
		XOR_BP,
		XOR_R13,
	};
	int32		off;			// Frame relative offset
	uint16		reg;			// Register index
	Type		cookietype;		// Type of the cookie
	uint8		flags;			// Flags describing this cookie
};

struct DISCARDEDSYM : SYMTYPE { // S_DISCARDED
	enum discarded_e {
		DISCARDED_UNKNOWN,
		DISCARDED_NOT_SELECTED,
		DISCARDED_NOT_REFERENCED,
	};
	uint32		discarded: 8;	// discarded_e
	uint32		reserved: 24;	// Unused
	uint32		fileid;			// First FILEID if line number info present
	uint32		linenum;		// First line number
	char		data[];			// Original record(s) with invalid type indices
};

struct REFFLAGS {
	union {
		uint16		all;
		struct {
			uint16		fLocal		: 1;	// reference to local (vs. global) func or data
			uint16		fData		: 1;	// reference to data (vs. func)
			uint16		fUDT		: 1;	// reference to UDT
			uint16		fLabel		: 1;	// reference to label
			uint16		fConst		: 1;	// reference to const
			uint16		fUnk0		: 1;	// unknown
			uint16		fNamespace	: 1;	// reference to namespace
			uint16		reserved	: 9;	// reserved, must be zero
		};
	};
};

struct REFMINIPDB : SYMTYPE { // S_REF_MINIPDB
	union {
		uint32		isectCoff;	// coff section
		type_t		typind;		// type index
	};
	uint16			imod;		// mod index
	REFFLAGS		flags;
	embedded_string	name;		// zero terminated name string
};
struct PDBMAP : SYMTYPE { // S_PDBMAP
	char16		names[];	// zero terminated source PDB filename followed by zero terminated destination PDB filename, both in wchar_t
};

struct FASTLINKSYM : SYMTYPE { // S_FASTLINK (AJS)
	union {
		uint32		isectCoff;	// coff section
		type_t		typind;		// type index
	};
	REFFLAGS		flags;
	embedded_string	name;
};

//-----------------------------------------------------------------------------
// ComboID
//-----------------------------------------------------------------------------

struct XFIXUP_DATA {
	uint16		wType;
	uint16		wExtra;
	uint32		rva;
	uint32		rvaTarget;
};

// Those cross scope IDs are used to delay the ID merging for frontend and backend linker
// They avoid the copy type tree in some scenarios
struct ComboID {
	union {
		uint32	combo;
		struct {uint32 index:20, imod:12; };
	};
	ComboID(uint16 imod, uint32 index) : index(index), imod(imod) {}
	ComboID(uint32 combo) : combo(combo) {}
	operator uint32()			const { return combo; }
};

struct CrossScopeId {
	union {
		uint32	cross_scope;
		struct {uint32 local:20, scope:11, cross:1; };
	};
	CrossScopeId(uint32 i) : cross_scope(i) {}
	static bool IsCrossScopeId(uint32 i) { return CrossScopeId(i).cross; }
	CrossScopeId(uint16 scope, uint32 local) : local(local), scope(scope) {}
	operator uint32()		const	{ return cross_scope; }
};

// Combined encoding of TI or FuncId, In compiler implementation Id prefixed by 1 if it is function ID
struct DecoratedItemId {
	union {
		uint32	combo;
		struct {uint32 id:31, func:1;};
	};
	DecoratedItemId(bool func, ItemId id)	: id(id), func(func) {}
	DecoratedItemId(ItemId encodedId)		: id(encodedId) {}
	operator uint32()		const	{ return combo; }
};

//-----------------------------------------------------------------------------
// Location
//-----------------------------------------------------------------------------

// The binary annotation mechanism supports recording a list of annotations in an instruction stream
// The X64 unwind code and the DWARF standard have similar design
// One annotation contains opcode and a number of 32bits operands
// The initial set of annotation instructions are for line number table encoding only
// These annotations append to S_INLINESITE record, and operands are unsigned except for BA_OP_ChangeLineOffset
enum BinaryAnnotationOpcode {
	BA_OP_Invalid,							// 0	link time pdb contains PADDINGs
	BA_OP_CodeOffset,						// 1	param : start offset
	BA_OP_ChangeCodeOffsetBase,				// 2	param : nth separated code chunk (main code chunk == 0)
	BA_OP_ChangeCodeOffset,					// 3	param : delta of offset
	BA_OP_ChangeCodeLength,					// 4	param : length of code, default next start
	BA_OP_ChangeFile,						// 5	param : fileId
	BA_OP_ChangeLineOffset,					// 6	param : line offset (signed)
	BA_OP_ChangeLineEndDelta,				// 7	param : how many lines, default 1
	BA_OP_ChangeRangeKind,					// 8	param : either 1 (default, for statement) or 0 (for expression)
	BA_OP_ChangeColumnStart,				// 9	param : start column number, 0 means no column info
	BA_OP_ChangeColumnEndDelta,				// 10*	param : end column number delta (signed)
	//Combo opcodes for smaller encoding size
	BA_OP_ChangeCodeOffsetAndLineOffset,	// 11	param : ((sourceDelta << 4) | CodeDelta)
	BA_OP_ChangeCodeLengthAndCodeOffset,	// 12	param : codeLength, codeOffset
	BA_OP_ChangeColumnEnd,					// 13	param : end column number
};
inline int BinaryAnnotationInstructionOperandCount(BinaryAnnotationOpcode op) {
	return (op == BA_OP_ChangeCodeLengthAndCodeOffset) ? 2 : 1;
}

struct Location {
	uint32	offset, end_length, kind;
	uint32	line, col_start, col_end, chunk, file;
	Location()					{ clear(*this); }
	uint32	length()	const	{ return end_length ? end_length : this[1].offset - offset; }
	uint32	end()		const	{ return end_length ? offset + end_length : this[1].offset; }
};

struct LineNumberParser : const_memory_block {
	struct State : Location {
		int		num_lines;
		bool	dirty;

		void reset() { num_lines = 1; kind = 1; dirty = true; }

		const uint8	*next(const uint8 *p) {
			for (;;) {
				switch (*p++) {
					case BA_OP_ChangeRangeKind:					kind 		= CVUncompressData(p);	break;
					case BA_OP_ChangeLineEndDelta:				num_lines 	+= CVUncompressData(p);	break;
					case BA_OP_ChangeCodeOffsetBase:			chunk 		= CVUncompressData(p);	break;
					case BA_OP_ChangeCodeLength:				end_length 	= CVUncompressData(p);	return p;
					case BA_OP_ChangeCodeLengthAndCodeOffset:	end_length	= CVUncompressData(p); offset += CVUncompressData(p); return p;
					case BA_OP_CodeOffset:						offset 		= CVUncompressData(p);	break;
					case BA_OP_ChangeCodeOffset:				offset		+= CVUncompressData(p); end_length = 0; return p;
					case BA_OP_ChangeFile:						file 		= CVUncompressData(p);	break;
					case BA_OP_ChangeLineOffset:				line		+= DecodeSignedInt32(CVUncompressData(p));	break;
					case BA_OP_ChangeColumnStart:				col_start 	= CVUncompressData(p);	break;
					case BA_OP_ChangeColumnEndDelta:			col_end		+= DecodeSignedInt32(CVUncompressData(p)); break;
					case BA_OP_ChangeCodeOffsetAndLineOffset: {
						uint8 value = *p++;
						offset	+= value & 15;
						line	+= DecodeSignedInt32(value >> 4);
						break;
					}
					case BA_OP_ChangeColumnEnd:					col_end		= CVUncompressData(p);	break;
					case BA_OP_Invalid:							return 0;
					default:									ISO_ASSERT(0);
				}
			}
		}
	};
	mutable State state;

	struct iterator {
		typedef const Location element, &reference;
		State				*state;
		const uint8			*p;

		void	clean() {
			if (state->dirty) {
				p = state->next(p);
				state->dirty = false;
			}
		}

		iterator(State *state, const uint8 *p)	: state(state), p(p) {}
		reference	operator*()			{ clean(); return *state; }
		iterator&	operator++()		{ clean(); state->dirty = true; return *this; }
		bool		operator!=(const iterator &b) {
			if (!state->dirty)
				return true;
			for (const uint8 *p2 = p; p2 != b.p; ++p2) {
				if (*p2 != 0)
					return true;
			}
			return false;
		}
	};

	LineNumberParser(const const_memory_block &data, uint32 offset) : const_memory_block(data) { state.offset = offset; }
	iterator	begin()	const { state.reset(); return iterator(&state, const_memory_block::begin()); }
	iterator	end()	const { return iterator(0, const_memory_block::end()); }
};

inline LineNumberParser	INLINESITESYM::lines(uint32 offset)		const { return LineNumberParser(const_memory_block(binaryAnnotations, (const uint8*)next() - binaryAnnotations), offset); }
inline LineNumberParser	INLINESITESYM2::lines(uint32 offset)	const { return LineNumberParser(const_memory_block(binaryAnnotations, (const uint8*)next() - binaryAnnotations), offset); }

//-----------------------------------------------------------------------------
// Subsections
//-----------------------------------------------------------------------------

enum DEBUG_S_SUBSECTION_TYPE {
	DEBUG_S_IGNORE				= 0x80000000,	// if this bit is set in a subsection type then ignore the subsection contents
	DEBUG_S_SYMBOLS				= 0xf1,
	DEBUG_S_LINES				= 0xf2,
	DEBUG_S_STRINGTABLE			= 0xf3,
	DEBUG_S_FILECHKSMS			= 0xf4,
	DEBUG_S_FRAMEDATA			= 0xf5,
	DEBUG_S_INLINEELINES		= 0xf6,
	DEBUG_S_CROSSSCOPEIMPORTS	= 0xf7,
	DEBUG_S_CROSSSCOPEEXPORTS	= 0xf8,
	DEBUG_S_IL_LINES			= 0xf9,
	DEBUG_S_FUNC_MDTOKEN_MAP	= 0xfa,
	DEBUG_S_TYPE_MDTOKEN_MAP	= 0xfb,
	DEBUG_S_MERGED_ASSEMBLYINPUT= 0xfc,
	DEBUG_S_COFF_SYMBOL_RVA		= 0xfd,
};

struct SubSection {
	uint32		type;	//DEBUG_S_SUBSECTION_TYPE
	int32		cbLen;
	const void*						end()	const	{ return (const uint8*)(this + 1) +  cbLen; }
	const_memory_block				data()	const	{ return const_memory_block(this + 1, cbLen); }
	const SubSection*				next()	const	{ return (const SubSection*)((const uint8*)(this + 1) +  align(cbLen, 4)); }
	template<typename T> const T*	as()	const	{ return static_cast<const T*>(this); }
	template<DEBUG_S_SUBSECTION_TYPE T> auto as() const;
};

template<DEBUG_S_SUBSECTION_TYPE type> struct SubSectionT : SubSection {};
template<DEBUG_S_SUBSECTION_TYPE T> auto SubSection::as()	const	{ return static_cast<const SubSectionT<T>*>(this); }

template<DEBUG_S_SUBSECTION_TYPE type> auto GetFirstSubStream(const range<next_iterator<const SubSection>> &subs) {
	for (auto &j : subs) {
		if (j.type == type)
			return j.as<SubSectionT<type>>();
	}
	return (const SubSectionT<type>*)nullptr;
}

//f1
template<> struct SubSectionT<DEBUG_S_SYMBOLS> : SubSection {
	auto	entries() const { return make_next_range<SYMTYPE>(data()); }
};

//f2
template<> struct SubSectionT<DEBUG_S_LINES> : SubSection {
	enum {
		HAVE_COLUMNS	= 0x0001,
		ALWAYS_STEPINTO	= 0xfeefee,
		NEVER_STEPINTO	= 0xf00f00,
	};

	struct line {
		uint32	offset;					// Offset to start of code bytes for line number
		uint32	linenumStart	: 24;	// line where statement/expression starts
		uint32	deltaLineEnd	: 7;	// delta to line where statement ends (optional)
		uint32	fStatement		: 1;	// true if a statement linenumber, else an expression line num
		bool	normal() const { return linenumStart < NEVER_STEPINTO; }
	};
	struct column {
		uint16		offColumnStart;
		uint16		offColumnEnd;
	};
	struct entry {
		int32	offFile;
		int32	nLines;
		int32	cbBlock;
		line	line[1];
		auto	lines()		const { return make_range_n(line, nLines); }
		auto	next()		const { return (const entry*)&line[nLines]; }
	};
	struct entry_ex : entry {
		auto	columns()	const { return make_range_n((column*)&line[nLines], nLines); }
		auto	entries()	const { return make_tuple_range(lines(), columns()); }
		auto	next()		const { return (const entry_ex*)columns().end(); }
	};
	segmented32	addr;
	uint16		flags;
	int32		cb;

	segmented32	end_addr()	const { return addr + cb; }
	bool	extended()		const { return flags & HAVE_COLUMNS; }
	auto	entries()		const { return make_next_range((const entry*)(this + 1), (const entry*)next()); }
	auto	entries_ex()	const { ISO_ASSERT(extended()); return make_next_range((const entry_ex*)(this + 1), (const entry_ex*)next()); }
};

//f3
template<> struct SubSectionT<DEBUG_S_STRINGTABLE> : SubSection {
	auto	entries()				const { return make_next_range<embedded_string>(data()); }
	const char	*at(uint32 offset)	const { return data() + offset; }
};

//f4
template<> struct SubSectionT<DEBUG_S_FILECHKSMS> : SubSection {
	struct entry {
		uint32	name;
		uint8	cbChecksum;
		uint8	ChecksumType;	//SourceChksum_t
		auto	next()	const { return (const entry*)align((uint8*)(this + 1) + cbChecksum, 4); }
	};

	auto	entries()				const { return make_next_range<entry>(data()); }
	const entry	*at(uint32 offset)	const { return data() + offset; }
};

//f5
template<> struct SubSectionT<DEBUG_S_FRAMEDATA> : SubSection {
	struct entry {
		uint32		ulRvaStart;
		uint32		cbBlock;
		uint32		cbLocals;
		uint32		cbParams;
		uint32		cbStkMax;
		uint32		frameFunc;
		uint16		cbProlog;
		uint16		cbSavedRegs;
		uint32		fHasSEH:1, fHasEH:1, fIsFunctionStart:1, reserved:29;
	};
	uint32	unk;

	auto	entries()		const { return make_range<entry>(data() + 4); }
};

//f6 - //List start source file information for an inlined function
template<> struct SubSectionT<DEBUG_S_INLINEELINES> : SubSection {
	enum {
		SIGNATURE		= 0x0,
		SIGNATURE_EX	= 0x1,
	};
	struct entry {
		ItemId	inlinee;		// function id
		int32	fileId;			// offset into file table DEBUG_S_FILECHKSMS
		int32	sourceLineNum;	// definition start line number
	};
	struct entry_ex : entry {
		uint32		countOfExtraFiles;
		int32	extraFileId[1];
		auto	files()	const { return make_range_n(extraFileId, countOfExtraFiles); }
		auto	next()	const { return (const entry_ex*)&extraFileId[countOfExtraFiles]; }
	};

	struct iterator {
		const entry	*e;
		bool	extended;
		iterator(const entry *e, bool extended) : e(e), extended(extended) {}
		iterator&		operator++()					{ e = extended ? ((entry_ex*)e)->next() : e + 1; return *this; }
		bool			operator!=(const iterator& b)	{ return e != b.e; }
		const entry&	operator*()	const				{ return *e; }
	};

	uint32	signature;	// 0 seems to indicate no files, 1 indicates files

	bool	extended()		const { return signature == SIGNATURE_EX; }
	auto	entries()		const { auto data2 = data() + 4; return make_range<iterator>(iterator(data2, extended()), iterator(data2.end(), extended())); }
	auto	entries_ex()	const { ISO_ASSERT(extended()); return make_next_range<entry_ex>(data() + 4); }
};

//f7 - array of all imports by import module; list all cross reference for a specific ID scope
template<> struct SubSectionT<DEBUG_S_CROSSSCOPEIMPORTS> : SubSection {
	struct entry {
		int32	externalScope;			// Module of definition Scope (offset to ObjectFilePath)
		uint32		countOfCrossReferences;	// Count of following array.
		ItemId	referenceIds[1];		// ItemId in another compilation unit
		auto	refs()	const { return make_range_n(referenceIds, countOfCrossReferences); }
		auto	next()	const { return (const entry*)&referenceIds[countOfCrossReferences]; }
	};

	auto	entries()	const { return make_next_range<entry>(data()); }
};

//f8 - array of all exports in this module
template<> struct SubSectionT<DEBUG_S_CROSSSCOPEEXPORTS> : SubSection {
	struct entry {
		ItemId	localId;	// local id inside the compile time PDB scope. 0 based
		ItemId	globalId;	// global id inside the link time PDB scope, if scope are different
	};

	auto	entries()	const { return make_range<entry>(data()); }
};

//f9 - same as f2
template<> struct SubSectionT<DEBUG_S_IL_LINES> : SubSectionT<DEBUG_S_LINES> {};

//fa
template<> struct SubSectionT<DEBUG_S_FUNC_MDTOKEN_MAP> : SubSection {
	// AJS
	struct entry {
		uint32	a, b;
	};

	uint32	num_entries;
	entry	e[1];
	auto	entries()	const { return make_range_n(e, num_entries); }
};

//fb - same as fa
template<> struct SubSectionT<DEBUG_S_TYPE_MDTOKEN_MAP> : SubSectionT<DEBUG_S_FUNC_MDTOKEN_MAP> {};

//fc
template<> struct SubSectionT<DEBUG_S_MERGED_ASSEMBLYINPUT> : SubSection {};

//fd
template<> struct SubSectionT<DEBUG_S_COFF_SYMBOL_RVA> : SubSection {
	auto	entries()	const { return make_range<uint32>(data()); }
};


//-----------------------------------------------------------------------------
// Type dispatch function
//-----------------------------------------------------------------------------

template<typename R, typename P> R process(const Leaf *type, P &&proc) {
	switch (type->leaf) {
		case LF_MODIFIER_16t:		return proc(*type->as<Modifier_16t>());
		case LF_POINTER_16t:		return proc(*type->as<Pointer_16t>());
		case LF_ARRAY_16t:			return proc(*type->as<Array_16t>());
		case LF_CLASS_16t:			return proc(*type->as<Class_16t>());
		case LF_STRUCTURE_16t:		return proc(*type->as<Structure_16t>());
		case LF_UNION_16t:			return proc(*type->as<Union_16t>());
		case LF_ENUM_16t:			return proc(*type->as<Enum_16t>());
		case LF_PROCEDURE_16t:		return proc(*type->as<Proc_16t>());
		case LF_MFUNCTION_16t:		return proc(*type->as<MFunc_16t>());
		case LF_VTSHAPE:			return proc(*type->as<VTShape>());
		case LF_COBOL0_16t:			return proc(*type->as<Cobol0_16t>());
		case LF_COBOL1:				return proc(*type->as<Cobol1>());
		case LF_BARRAY_16t:			return proc(*type->as<BArray_16t>());
		case LF_LABEL:				return proc(*type->as<Label>());
//		case LF_NULL:
//		case LF_NOTTRAN:
		case LF_DIMARRAY_16t:		return proc(*type->as<DimArray_16t>());
		case LF_VFTPATH_16t:		return proc(*type->as<VFTPath_16t>());
		case LF_PRECOMP_16t:		return proc(*type->as<PreComp_16t>());
		case LF_ENDPRECOMP:			return proc(*type->as<EndPreComp>());
//		case LF_OEM_16t:
//		case LF_TYPESERVER_ST:
		case LF_SKIP_16t:			return proc(*type->as<Skip_16t>());
		case LF_ARGLIST_16t:		return proc(*type->as<ArgList_16t>());
		case LF_DEFARG_16t:			return proc(*type->as<DefArg_16t>());
		case LF_LIST:				return proc(*type->as<List>());
		case LF_FIELDLIST_16t:		return proc(*type->as<FieldList_16t>());
		case LF_DERIVED_16t:		return proc(*type->as<Derived_16t>());
		case LF_BITFIELD_16t:		return proc(*type->as<Bitfield_16t>());
		case LF_METHODLIST_16t:		return proc(*type->as<MethodList_16t>());
		case LF_DIMCONU_16t:		return proc(*type->as<DimCon_16t>());
		case LF_DIMCONLU_16t:		return proc(*type->as<DimCon_16t>());
		case LF_DIMVARU_16t:		return proc(*type->as<DimVar_16t>());
		case LF_DIMVARLU_16t:		return proc(*type->as<DimVar_16t>());
		case LF_REFSYM:				return proc(*type->as<RefSym>());
		case LF_BCLASS_16t:			return proc(*type->as<BClass_16t>());
		case LF_VBCLASS_16t:		return proc(*type->as<VBClass_16t>());
		case LF_IVBCLASS_16t:		return proc(*type->as<VBClass_16t>());
		case LF_ENUMERATE_ST:		return proc(*type->as<Enumerate>(), true);
		case LF_FRIENDFCN_16t:		return proc(*type->as<FriendFcn_16t>());
		case LF_INDEX_16t:			return proc(*type->as<Index_16t>());
		case LF_MEMBER_16t:			return proc(*type->as<Member_16t>());
		case LF_STMEMBER_16t:		return proc(*type->as<STMember_16t>());
		case LF_METHOD_16t:			return proc(*type->as<Method_16t>());
		case LF_NESTTYPE_16t:		return proc(*type->as<NestType_16t>());
		case LF_VFUNCTAB_16t:		return proc(*type->as<VFuncTab_16t>());
		case LF_FRIENDCLS_16t:		return proc(*type->as<FriendCls_16t>());
		case LF_ONEMETHOD_16t:		return proc(*type->as<OneMethod_16t>());
		case LF_VFUNCOFF_16t:		return proc(*type->as<VFuncOff_16t>());
		case LF_MODIFIER:			return proc(*type->as<Modifier>());
		case LF_POINTER:			return proc(*type->as<Pointer>());
		case LF_ARRAY_ST:			return proc(*type->as<Array>());
		case LF_CLASS_ST:			return proc(*type->as<Class>(), true);
		case LF_STRUCTURE_ST:		return proc(*type->as<Structure>(), true);
		case LF_UNION_ST:			return proc(*type->as<Union>(), true);
		case LF_ENUM_ST:			return proc(*type->as<Enum>(), true);
		case LF_PROCEDURE:			return proc(*type->as<Proc>());
		case LF_MFUNCTION:			return proc(*type->as<MFunc>());
		case LF_COBOL0:				return proc(*type->as<Cobol0>());
		case LF_BARRAY:				return proc(*type->as<BArray>());
		case LF_DIMARRAY_ST:		return proc(*type->as<DimArray>(), true);
		case LF_VFTPATH:			return proc(*type->as<VFTPath>());
		case LF_PRECOMP_ST:			return proc(*type->as<PreComp>(), true);
//		case LF_OEM:
		case LF_ALIAS_ST:			return proc(*type->as<Alias>(), true);
//		case LF_OEM2:
		case LF_SKIP:				return proc(*type->as<Skip>());
		case LF_ARGLIST:			return proc(*type->as<ArgList>());
		case LF_DEFARG_ST:			return proc(*type->as<DefArg>());
		case LF_FIELDLIST:			return proc(*type->as<FieldList>());
		case LF_DERIVED:			return proc(*type->as<Derived>());
		case LF_BITFIELD:			return proc(*type->as<Bitfield>());
		case LF_METHODLIST:			return proc(*type->as<MethodList>());
		case LF_DIMCONU:			return proc(*type->as<DimCon>());
		case LF_DIMCONLU:			return proc(*type->as<DimCon>());
		case LF_DIMVARU:			return proc(*type->as<DimVar>());
		case LF_DIMVARLU:			return proc(*type->as<DimVar>());
		case LF_BCLASS:				return proc(*type->as<BClass>());
		case LF_VBCLASS:			return proc(*type->as<VBClass>());
		case LF_IVBCLASS:			return proc(*type->as<VBClass>());
		case LF_FRIENDFCN_ST:		return proc(*type->as<FriendFcn>(), true);
		case LF_INDEX:				return proc(*type->as<Index>());
		case LF_MEMBER_ST:			return proc(*type->as<Member>(), true);
		case LF_STMEMBER_ST:		return proc(*type->as<STMember>(), true);
		case LF_METHOD_ST:			return proc(*type->as<Method>(), true);
		case LF_NESTTYPE_ST:		return proc(*type->as<NestType>(), true);
		case LF_VFUNCTAB:			return proc(*type->as<VFuncTab>());
		case LF_FRIENDCLS:			return proc(*type->as<FriendCls>());
		case LF_ONEMETHOD_ST:		return proc(*type->as<OneMethod>(), true);
		case LF_VFUNCOFF:			return proc(*type->as<VFuncOff>());
		case LF_NESTTYPEEX_ST:		return proc(*type->as<NestTypeEx>(), true);
		case LF_MEMBERMODIFY_ST:	return proc(*type->as<MemberModify>(), true);
		case LF_MANAGED_ST:			return proc(*type->as<Managed>(), true);
		case LF_TYPESERVER:			return proc(*type->as<TypeServer>());
		case LF_ENUMERATE:			return proc(*type->as<Enumerate>());
		case LF_ARRAY:				return proc(*type->as<Array>());
		case LF_CLASS:				return proc(*type->as<Class>());
		case LF_STRUCTURE:			return proc(*type->as<Structure>());
		case LF_UNION:				return proc(*type->as<Union>());
		case LF_ENUM:				return proc(*type->as<Enum>());
		case LF_DIMARRAY:			return proc(*type->as<DimArray>());
		case LF_PRECOMP:			return proc(*type->as<PreComp>());
		case LF_ALIAS:				return proc(*type->as<Alias>());
		case LF_DEFARG:				return proc(*type->as<DefArg>());
		case LF_FRIENDFCN:			return proc(*type->as<FriendFcn>());
		case LF_MEMBER:				return proc(*type->as<Member>());
		case LF_STMEMBER:			return proc(*type->as<STMember>());
		case LF_METHOD:				return proc(*type->as<Method>());
		case LF_NESTTYPE:			return proc(*type->as<NestType>());
		case LF_ONEMETHOD:			return proc(*type->as<OneMethod>());
		case LF_NESTTYPEEX:			return proc(*type->as<NestTypeEx>());
		case LF_MEMBERMODIFY:		return proc(*type->as<MemberModify>());
		case LF_MANAGED:			return proc(*type->as<Managed>());
		case LF_TYPESERVER2:		return proc(*type->as<TypeServer2>());
		case LF_STRIDED_ARRAY:		return proc(*type->as<StridedArray>());
		case LF_HLSL:				return proc(*type->as<HLSL>());
		case LF_MODIFIER_EX:		return proc(*type->as<ModifierEx>());
		case LF_INTERFACE:			return proc(*type->as<Interface>());
		case LF_BINTERFACE:			return proc(*type->as<BInterface>());
		case LF_VECTOR:				return proc(*type->as<Vector>());
		case LF_MATRIX:				return proc(*type->as<Matrix>());
		case LF_VFTABLE:			return proc(*type->as<Vftable>());
		case LF_FUNC_ID:			return proc(*type->as<FuncId>());
		case LF_MFUNC_ID:			return proc(*type->as<MFuncId>());
		case LF_BUILDINFO:			return proc(*type->as<BuildInfo>());
		case LF_SUBSTR_LIST:		return proc(*type->as<ArgList>());
		case LF_STRING_ID:			return proc(*type->as<StringId>());
		case LF_UDT_SRC_LINE:		return proc(*type->as<UdtSrcLine>());
		case LF_UDT_MOD_SRC_LINE:	return proc(*type->as<UdtModSrcLine>());
		default:					return proc(*type);
	}
}

//-----------------------------------------------------------------------------
// Symbol dispatch function
//-----------------------------------------------------------------------------

template<typename R, typename P> R process(const SYMTYPE *sym, P &&proc) {
	switch (sym->rectyp) {
		case S_COMPILE:				return proc(*sym->as<CFLAGSYM>());
		case S_REGISTER_16t:		return proc(*sym->as<REGSYM_16t>());
		case S_CONSTANT_16t:		return proc(*sym->as<CONSTSYM_16t>());
		case S_UDT_16t:				return proc(*sym->as<UDTSYM_16t>());
		case S_SSEARCH:				return proc(*sym->as<SEARCHSYM>());
		case S_END:					return proc(*sym->as<ENDSYM>());
//		case S_SKIP:
//		case S_CVRESERVE:
//		case S_OBJNAME_ST:
		case S_ENDARG:				return proc(*sym->as<ENDARGSYM>());
		case S_COBOLUDT_16t:		return proc(*sym->as<UDTSYM_16t>());
		case S_MANYREG_16t:			return proc(*sym->as<MANYREGSYM_16t>());
		case S_RETURN:				return proc(*sym->as<RETURNSYM>());
		case S_ENTRYTHIS:			return proc(*sym->as<ENTRYTHISSYM>());
		case S_BPREL16:				return proc(*sym->as<BPRELSYM16>());
//		case S_LDATA16:
//		case S_GDATA16:
		case S_PUB16:				return proc(*sym->as<PUBSYM16>());
		case S_LPROC16:				return proc(*sym->as<PROCSYM16>());
		case S_GPROC16:				return proc(*sym->as<PROCSYM16>());
		case S_THUNK16:				return proc(*sym->as<THUNKSYM16>());
		case S_BLOCK16:				return proc(*sym->as<BLOCKSYM16>());
		case S_WITH16:				return proc(*sym->as<WITHSYM16>());
		case S_LABEL16:				return proc(*sym->as<LABELSYM16>());
		case S_CEXMODEL16:			return proc(*sym->as<CEXMSYM16>());
//		case S_VFTABLE16:
		case S_REGREL16:			return proc(*sym->as<REGREL16>());
		case S_BPREL32_16t:			return proc(*sym->as<BPRELSYM32_16t>());
		case S_LDATA32_16t:			return proc(*sym->as<DATASYM32_16t>());
		case S_GDATA32_16t:			return proc(*sym->as<DATASYM32_16t>());
		case S_PUB32_16t:			return proc(*sym->as<PUBSYM32_16t>());
		case S_LPROC32_16t:			return proc(*sym->as<PROCSYM32_16t>());
		case S_GPROC32_16t:			return proc(*sym->as<PROCSYM32_16t>());
//		case S_THUNK32_ST:
//		case S_BLOCK32_ST:
//		case S_WITH32_ST:
//		case S_LABEL32_ST:
		case S_CEXMODEL32:			return proc(*sym->as<CEXMSYM32>());
		case S_VFTABLE32_16t:		return proc(*sym->as<VPATHSYM32_16t>());
		case S_REGREL32_16t:		return proc(*sym->as<REGREL32_16t>());
		case S_LTHREAD32_16t:		return proc(*sym->as<THREADSYM32_16t>());
		case S_GTHREAD32_16t:		return proc(*sym->as<THREADSYM32_16t>());
		case S_SLINK32:				return proc(*sym->as<SLINK32>());
		case S_LPROCMIPS_16t:		return proc(*sym->as<PROCSYMMIPS_16t>());
		case S_GPROCMIPS_16t:		return proc(*sym->as<PROCSYMMIPS_16t>());
		case S_PROCREF_ST:			return proc(*sym->as<REFSYM>(), true);
		case S_DATAREF_ST:			return proc(*sym->as<REFSYM>(), true);
		case S_ALIGN:				return proc(*sym->as<ALIGNSYM>());
		case S_LPROCREF_ST:			return proc(*sym->as<REFSYM>(), true);
//		case S_OEM:
		case S_REGISTER_ST:			return proc(*sym->as<REGSYM>(), true);
		case S_CONSTANT_ST:			return proc(*sym->as<CONSTSYM>(), true);
		case S_UDT_ST:				return proc(*sym->as<UDTSYM>(), true);
		case S_COBOLUDT_ST:			return proc(*sym->as<UDTSYM>(), true);
		case S_MANYREG_ST:			return proc(*sym->as<MANYREGSYM>(), true);
		case S_BPREL32_ST:			return proc(*sym->as<BPRELSYM32>(), true);
		case S_LDATA32_ST:			return proc(*sym->as<DATASYM32>(), true);
		case S_GDATA32_ST:			return proc(*sym->as<DATASYM32>(), true);
		case S_PUB32_ST:			return proc(*sym->as<PUBSYM32>(), true);
		case S_LPROC32_ST:			return proc(*sym->as<PROCSYM32>(), true);
		case S_GPROC32_ST:			return proc(*sym->as<PROCSYM32>(), true);
		case S_VFTABLE32:			return proc(*sym->as<VPATHSYM32>());
		case S_REGREL32_ST:			return proc(*sym->as<REGREL32>(), true);
		case S_LTHREAD32_ST:		return proc(*sym->as<THREADSYM32>(), true);
		case S_GTHREAD32_ST:		return proc(*sym->as<THREADSYM32>(), true);
		case S_LPROCMIPS_ST:		return proc(*sym->as<PROCSYMMIPS>(), true);
		case S_GPROCMIPS_ST:		return proc(*sym->as<PROCSYMMIPS>(), true);
		case S_FRAMEPROC:			return proc(*sym->as<FRAMEPROCSYM>());
//		case S_COMPILE2_ST:
		case S_MANYREG2_ST:			return proc(*sym->as<MANYREGSYM2>(), true);
		case S_LPROCIA64_ST:		return proc(*sym->as<PROCSYMIA64>(), true);
		case S_GPROCIA64_ST:		return proc(*sym->as<PROCSYMIA64>(), true);
		case S_LOCALSLOT_ST:		return proc(*sym->as<SLOTSYM32>(), true);
		case S_PARAMSLOT_ST:		return proc(*sym->as<SLOTSYM32>(), true);
		case S_ANNOTATION:			return proc(*sym->as<ANNOTATIONSYM>());
		case S_GMANPROC_ST:			return proc(*sym->as<MANPROCSYM>(), true);
		case S_LMANPROC_ST:			return proc(*sym->as<MANPROCSYM>(), true);
		case S_LMANDATA_ST:			return proc(*sym->as<DATASYM32>(), true);
		case S_GMANDATA_ST:			return proc(*sym->as<DATASYM32>(), true);
		case S_MANFRAMEREL_ST:		return proc(*sym->as<FRAMERELSYM>(), true);
		case S_MANREGISTER_ST:		return proc(*sym->as<ATTRREGREL>(), true);
		case S_MANSLOT_ST:			return proc(*sym->as<ATTRSLOTSYM>(), true);
		case S_MANMANYREG_ST:		return proc(*sym->as<ATTRMANYREGSYM>(), true);
		case S_MANREGREL_ST:		return proc(*sym->as<ATTRREGREL>(), true);
		case S_MANMANYREG2_ST:		return proc(*sym->as<ATTRMANYREGSYM2>(), true);
		case S_MANTYPREF:			return proc(*sym->as<MANTYPREF>());
		case S_UNAMESPACE_ST:		return proc(*sym->as<UNAMESPACE>(), true);
		case S_OBJNAME:				return proc(*sym->as<OBJNAMESYM>());
		case S_THUNK32:				return proc(*sym->as<THUNKSYM32>());
		case S_BLOCK32:				return proc(*sym->as<BLOCKSYM32>());
		case S_WITH32:				return proc(*sym->as<WITHSYM32>());
		case S_LABEL32:				return proc(*sym->as<LABELSYM32>());
		case S_REGISTER:			return proc(*sym->as<REGSYM>());
		case S_CONSTANT:			return proc(*sym->as<CONSTSYM>());
		case S_UDT:					return proc(*sym->as<UDTSYM>());
		case S_COBOLUDT:			return proc(*sym->as<UDTSYM>());
		case S_MANYREG:				return proc(*sym->as<MANYREGSYM>());
		case S_BPREL32:				return proc(*sym->as<BPRELSYM32>());
		case S_LDATA32:				return proc(*sym->as<DATASYM32>());
		case S_GDATA32:				return proc(*sym->as<DATASYM32>());
		case S_PUB32:				return proc(*sym->as<PUBSYM32>());
		case S_LPROC32:				return proc(*sym->as<PROCSYM32>());
		case S_GPROC32:				return proc(*sym->as<PROCSYM32>());
		case S_REGREL32:			return proc(*sym->as<REGREL32>());
		case S_LTHREAD32:			return proc(*sym->as<THREADSYM32>());
		case S_GTHREAD32:			return proc(*sym->as<THREADSYM32>());
		case S_LPROCMIPS:			return proc(*sym->as<PROCSYMMIPS>());
		case S_GPROCMIPS:			return proc(*sym->as<PROCSYMMIPS>());
		case S_COMPILE2:			return proc(*sym->as<COMPILESYM>());
		case S_MANYREG2:			return proc(*sym->as<MANYREGSYM2>());
		case S_LPROCIA64:			return proc(*sym->as<PROCSYMIA64>());
		case S_GPROCIA64:			return proc(*sym->as<PROCSYMIA64>());
//		case S_LOCALSLOT:
		case S_SLOT:				return proc(*sym->as<SLOTSYM32>());
		case S_PARAMSLOT:			return proc(*sym->as<ANNOTATIONSYM>());
		case S_LMANDATA:			return proc(*sym->as<DATASYM32>());
		case S_GMANDATA:			return proc(*sym->as<DATASYM32>());
		case S_MANFRAMEREL:			return proc(*sym->as<FRAMERELSYM>());
		case S_MANREGISTER:			return proc(*sym->as<ATTRREGREL>());
		case S_MANSLOT:				return proc(*sym->as<ATTRSLOTSYM>());
		case S_MANMANYREG:			return proc(*sym->as<ATTRMANYREGSYM>());
		case S_MANREGREL:			return proc(*sym->as<ATTRREGREL>());
		case S_MANMANYREG2:			return proc(*sym->as<ATTRMANYREGSYM2>());
		case S_UNAMESPACE:			return proc(*sym->as<UNAMESPACE>());
		case S_PROCREF:				return proc(*sym->as<REFSYM2>());
		case S_DATAREF:				return proc(*sym->as<REFSYM2>());
		case S_LPROCREF:			return proc(*sym->as<REFSYM2>());
//		case S_ANNOTATIONREF:
//		case S_TOKENREF:
		case S_GMANPROC:			return proc(*sym->as<MANPROCSYM>());
		case S_LMANPROC:			return proc(*sym->as<MANPROCSYM>());
		case S_TRAMPOLINE:			return proc(*sym->as<TRAMPOLINESYM>());
		case S_MANCONSTANT:			return proc(*sym->as<CONSTSYM>());
		case S_ATTR_FRAMEREL:		return proc(*sym->as<FRAMERELSYM>());
		case S_ATTR_REGISTER:		return proc(*sym->as<ATTRREGSYM>());
		case S_ATTR_REGREL:			return proc(*sym->as<ATTRREGREL>());
		case S_ATTR_MANYREG:		return proc(*sym->as<ATTRMANYREGSYM2>());
		case S_SEPCODE:				return proc(*sym->as<SEPCODESYM>());
//		case S_LOCAL_2005:
//		case S_DEFRANGE_2005:
//		case S_DEFRANGE2_2005:
		case S_SECTION:				return proc(*sym->as<SECTIONSYM>());
		case S_COFFGROUP:			return proc(*sym->as<COFFGROUPSYM>());
		case S_EXPORT:				return proc(*sym->as<EXPORTSYM>());
		case S_CALLSITEINFO:		return proc(*sym->as<CALLSITEINFO>());
		case S_FRAMECOOKIE:			return proc(*sym->as<FRAMECOOKIE>());
		case S_DISCARDED:			return proc(*sym->as<DISCARDEDSYM>());
		case S_COMPILE3:			return proc(*sym->as<COMPILESYM3>());
		case S_ENVBLOCK:			return proc(*sym->as<ENVBLOCKSYM>());
		case S_LOCAL:				return proc(*sym->as<LOCALSYM>());
		case S_DEFRANGE:			return proc(*sym->as<DEFRANGESYM>());
		case S_DEFRANGE_SUBFIELD:	return proc(*sym->as<DEFRANGESYMSUBFIELD>());
		case S_DEFRANGE_REGISTER:	return proc(*sym->as<DEFRANGESYMREGISTERREL>());
		case S_DEFRANGE_FRAMEPOINTER_REL:	return proc(*sym->as<DEFRANGESYMFRAMEPOINTERREL>());
		case S_DEFRANGE_SUBFIELD_REGISTER:	return proc(*sym->as<DEFRANGESYMSUBFIELDREGISTER>());
		case S_DEFRANGE_FRAMEPOINTER_REL_FULL_SCOPE: return proc(*sym->as<DEFRANGESYMFRAMEPOINTERREL_FULL_SCOPE>());
		case S_DEFRANGE_REGISTER_REL:		return proc(*sym->as<DEFRANGESYMREGISTERREL>());
		case S_LPROC32_ID:			return proc(*sym->as<PROCSYM32>());
		case S_GPROC32_ID:			return proc(*sym->as<PROCSYM32>());
		case S_LPROCMIPS_ID:		return proc(*sym->as<PROCSYM32>());
		case S_GPROCMIPS_ID:		return proc(*sym->as<PROCSYM32>());
		case S_LPROCIA64_ID:		return proc(*sym->as<PROCSYM32>());
		case S_GPROCIA64_ID:		return proc(*sym->as<PROCSYM32>());
		case S_BUILDINFO:			return proc(*sym->as<BUILDINFOSYM>());
		case S_INLINESITE:			return proc(*sym->as<INLINESITESYM>());
		case S_INLINESITE_END:		return proc(*sym->as<ENDSYM>());
		case S_PROC_ID_END:			return proc(*sym->as<ENDSYM>());
		case S_DEFRANGE_HLSL:		return proc(*sym->as<DEFRANGESYMHLSL>());
		case S_GDATA_HLSL:			return proc(*sym->as<DATASYMHLSL>());
		case S_LDATA_HLSL:			return proc(*sym->as<DATASYMHLSL>());
		case S_FILESTATIC:			return proc(*sym->as<FILESTATICSYM>());
		case S_ARMSWITCHTABLE:		return proc(*sym->as<ARMSWITCHTABLE>());
		case S_CALLEES:				return proc(*sym->as<FUNCTIONLIST>());
		case S_CALLERS:				return proc(*sym->as<FUNCTIONLIST>());
		case S_POGODATA:			return proc(*sym->as<POGOINFO>());
		case S_INLINESITE2:			return proc(*sym->as<INLINESITESYM2>());
		case S_HEAPALLOCSITE:		return proc(*sym->as<HEAPALLOCSITE>());
		case S_MOD_TYPEREF:			return proc(*sym->as<MODTYPEREF>());
		case S_REF_MINIPDB:			return proc(*sym->as<REFMINIPDB>());
		case S_PDBMAP:				return proc(*sym->as<PDBMAP>());
		case S_GDATA_HLSL32:		return proc(*sym->as<DATASYMHLSL32>());
		case S_LDATA_HLSL32:		return proc(*sym->as<DATASYMHLSL32>());
		case S_GDATA_HLSL32_EX:		return proc(*sym->as<DATASYMHLSL32_EX>());
		case S_LDATA_HLSL32_EX:		return proc(*sym->as<DATASYMHLSL32_EX>());
		case S_FASTLINK:			return proc(*sym->as<FASTLINKSYM>());
		case S_INLINEES:			return proc(*sym->as<FUNCTIONLIST>());
		default:					return proc(*sym);
	}
}

template<typename T, typename V=void> struct get_name_s {
	static const char *sz(const T &t) { return 0; }
};

template<typename T> struct get_name_s<T, typename T_void<decltype(&T::name)>::type> {
	template<typename N> struct get						{ static const char *f(const T &t) { return t.name; } };
	template<typename R> struct get<R (T::*)() const>	{ static const char *f(const T &t) { return t.name(); } };
	static const char*	helper(const T &t)				{ return get<decltype(&T::name)>::f(t); }

	static const char*	sz(const T &t)	{ return helper(t); }
	static auto			st(const T &t)	{ const char *p = helper(t); return str(p + 1, p[0]); }
};

struct name_getter {
	template<typename T> count_string	operator()(const T &t)			{ return count_string(get_name_s<T>::sz(t)); };
	template<typename T> count_string	operator()(const T &t, bool)	{ return get_name_s<T>::st(t); };
};

inline count_string get_name(const SYMTYPE *sym) {
	return process<count_string>(sym, name_getter());
}
inline count_string get_name(const Leaf *type) {
	switch (type->leaf) {
		case LF_VFUNCTAB:
		case LF_VFUNCTAB_16t:
		case LF_VFUNCOFF:
		case LF_VFUNCOFF_16t:	return "vfunctab";
		default:				return process<count_string>(type, name_getter());
	}
}

inline int64 get_offset(const Leaf *p) {
	switch (p->leaf) {
		case LF_BCLASS_16t:	return p->as<BClass_16t>()->offset;
		case LF_MEMBER_16t:	return p->as<Member_16t>()->offset;
		case LF_BCLASS:
		case LF_BINTERFACE:	return p->as<BClass>()->offset;
		case LF_MEMBER:		return  p->as<Member>()->offset;
	}
	return -1;
}

} //namespace CV

#pragma pack ( pop )
