#include "iso/iso_files.h"
#include "comms/zlib_stream.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	matlab v5
//-----------------------------------------------------------------------------

namespace MATLAB {
	enum TYPE {
		INT8		= 1,
		UINT8		= 2,
		INT16		= 3,
		UINT16		= 4,
		INT32		= 5,
		UINT32		= 6,
		SINGLE		= 7,
		DOUBLE		= 9,
		INT64		= 12,
		UINT64		= 13,
		MATRIX		= 14,
		COMPRESSED	= 15,
		UTF8		= 16,
		UTF16		= 17,
		UTF32		= 18,

		MAX_TYPE	= UTF8
	};

	enum FLAGS : uint8 {
		COMPLEX = 1 << 4,
		GLOBAL	= 1 << 5,
		LOGICAL	= 1 << 6,
	};

	enum CLASS : uint8 {
		CELL_CLASS		= 1,	//Cell array
		STRUCT_CLASS	= 2,	//Structure
		OBJECT_CLASS	= 3,	//Object
		CHAR_CLASS		= 4,	//Character array
		SPARSE_CLASS	= 5,	//Sparse array
		DOUBLE_CLASS	= 6,	//Double precision array
		SINGLE_CLASS	= 7,	//Single precision array
		INT8_CLASS		= 8,	//8 - bit, signed integer
		UINT8_CLASS		= 9,	//8 - bit, unsigned integer
		INT16_CLASS		= 10,	//16 - bit, signed integer
		UINT16_CLASS	= 11,	//16-bit, unsigned integer
		INT32_CLASS		= 12,	//32-bit, signed integer
		UINT32_CLASS	= 13,	//32-bit, unsigned integer
		INT64_CLASS		= 14,	//64-bit, signed integer
		UINT64_CLASS	= 15,	//64-bit, unsigned integer
	};

	uint8 type_size(TYPE t) {
		static const uint8 sizes[] = {
			0,
			1,	//INT8		= 1,
			1,	//UINT8		= 2,
			2,	//INT16		= 3,
			2,	//UINT16	= 4,
			4,	//INT32		= 5,
			4,	//UINT32	= 6,
			4,	//SINGLE	= 7,
			0,	//8
			8,	//DOUBLE	= 9,
			0,	//10
			0,	//11
			8,	//INT64		= 12,
			8,	//UINT64	= 13,
			0,	//MATRIX	= 14,
			0,	//COMPRESSED= 15,
			1,	//UTF8		= 16,
			2,	//UTF16		= 17,
			4,	//UTF32		= 18,
		};
		return t <= MAX_TYPE ? sizes[t] : 0;
	}

	struct header {
		char	text[116];
		uint32	susbsys_data_offset[2];
		uint16	version, endian;

		bool	valid() const {
			return str(text).begins("MATLAB") && version == 0x100 && (endian == 'IM' || endian == 'MI');
		}
		bool	bigendian() const {
			return endian == 'IM';
		}
	};

	template<bool be> struct _element;
	template<> struct _element<false> {
		union {
			struct { uint32	type, size;};
			struct { uint16 ctype, csize; uint32 cdata; };
		};
		bool	compressed()	const { return !!csize; }
	};
	template<> struct _element<true> {
		union {
			struct { bigendian0::uint32	type, size;};
			struct { bigendian0::uint16	csize, ctype; bigendian0::uint32 cdata; };
		};
		bool	compressed()	const { return !!ctype; }
	};

	template<bool be> struct element : _element<be> {
		TYPE	get_type()			const { return (TYPE)(compressed() ? native_endian(ctype) : native_endian(type)); }
		uint32	total_size()		const { return sizeof(*this) + (compressed() ? 0 : size); }
		const_memory_block	data()	const {
			return compressed() ? const_memory_block(&cdata, csize) : const_memory_block(this + 1, size);
		}
		malloc_block	data(istream_ref file)	const {
			if (compressed())
				return const_memory_block(&cdata, csize);
			return malloc_block(file, size);
		}
	};

	struct array_flags {
		CLASS		clss;
		FLAGS		flags;
		uint8		undef0;
		uint8		undef1;
		uint32		max_nz;	// for sparse arrays
	};
}

template<bool be, typename T> ISO_ptr<void> MakeBlock(const const_memory_block &data) {
	int	num = int(data.length() / sizeof(T));
	if (num == 1)
		return ISO_ptr<T>(0, *(const endian_t<T,be>*)data);
	else
		return ISO_ptr<ISO_openarray<T>>(0, make_range<endian_t<T,be>>(data));
}

template<bool be> ISO_ptr<void> MakeBlock(MATLAB::TYPE type, const const_memory_block &data) {
	switch (type) {
		case MATLAB::INT8:		return MakeBlock<be, int8>(data);
		case MATLAB::UINT8:		return MakeBlock<be, uint8>(data);
		case MATLAB::INT16:		return MakeBlock<be, int16>(data);
		case MATLAB::UINT16:	return MakeBlock<be, uint16>(data);
		case MATLAB::INT32:		return MakeBlock<be, int32>(data);
		case MATLAB::UINT32:	return MakeBlock<be, uint32>(data);
		case MATLAB::SINGLE:	return MakeBlock<be, float>(data);
		case MATLAB::DOUBLE:	return MakeBlock<be, double>(data);
		case MATLAB::INT64:		return MakeBlock<be, int64>(data);
		case MATLAB::UINT64:	return MakeBlock<be, uint64>(data);
		case MATLAB::UTF8:		return MakeBlock<be, char>(data);
		case MATLAB::UTF16:		return MakeBlock<be, char16>(data);
		case MATLAB::UTF32:		return MakeBlock<be, char32>(data);
		default:				return ISO::MakePtr(none, data);
	}
}

template<bool be> ISO_ptr<void> ReadRaw(tag id, istream_ref file, uint64 end = ~uint64(0)) {
	ISO_ptr<anything>	p(id);
	MATLAB::element<be>	e;
	streamptr			tell;
	while ((tell = file.tell()) < end && file.read(e)) {
		auto	type	= e.get_type();
		if (type > MATLAB::MAX_TYPE)
			break;

		ISO_ASSERT(type <= MATLAB::MAX_TYPE);
		if (type == MATLAB::MATRIX) {
			p->Append(ReadRaw<be>(none, file, file.tell() + e.size));

		} else if (type == MATLAB::COMPRESSED) {
			p->Append(ReadRaw<be>(none, zlib_reader(file).me()));
			file.seek(align(tell + e.size + 8, 8));

		} else {
			auto	data = e.data(file);
			if (!e.compressed())
				file.seek_cur(-data.length() & 7);
			p->Append(MakeBlock<be>(e.get_type(), data));
		}
	}
	return p;
}

class MATLAB5RawFileHandler : public FileHandler {
	const char*		GetDescription() override { return "MATLAB v5 (raw)"; }
	int				Check(istream_ref file) override {
		file.seek(0);
		MATLAB::header	h;
		return file.read(h) && h.valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		MATLAB::header	h;
		if (!file.read(h) || !h.valid())
			return ISO_NULL;

		return h.bigendian()
			? ReadRaw<true>(id, file)
			: ReadRaw<false>(id, file);
	}
} matlab5raw;

template<bool be, typename T, typename R> dynamic_array<T> GetBlock1(MATLAB::TYPE type, const R &data) {
	switch (type) {
		case MATLAB::INT8:		return make_range<endian_t<int8,   be>>(data);
		case MATLAB::UINT8:		return make_range<endian_t<uint8,  be>>(data);
		case MATLAB::INT16:		return make_range<endian_t<int16,  be>>(data);
		case MATLAB::UINT16:	return make_range<endian_t<uint16, be>>(data);
		case MATLAB::INT32:		return make_range<endian_t<int32,  be>>(data);
		case MATLAB::UINT32:	return make_range<endian_t<uint32, be>>(data);
		case MATLAB::SINGLE:	return make_range<endian_t<float,  be>>(data);
		case MATLAB::DOUBLE:	return make_range<endian_t<double, be>>(data);
		case MATLAB::INT64:		return make_range<endian_t<int64,  be>>(data);
		case MATLAB::UINT64:	return make_range<endian_t<uint64, be>>(data);
		case MATLAB::UTF8:		return make_range<endian_t<char,   be>>(data);
		case MATLAB::UTF16:		return make_range<endian_t<char16, be>>(data);
		case MATLAB::UTF32:		return make_range<endian_t<char32, be>>(data);
		default:				return none;
	}
}

template<bool be, typename T> dynamic_array<T> GetBlock(istream_ref file) {
	auto	e		= file.get<MATLAB::element<be>>();
	if (e.compressed()) {
		return GetBlock1<be, T>(e.get_type(), const_memory_block(&e.cdata, e.csize));

	} else {
		ISO_TRACEF("size = %i\n", e.size);
		auto	r = GetBlock1<be, T>(e.get_type(), make_reader_offset(file, e.size));
		ISO_ASSERT(r.size() == e.size / type_size(e.get_type()));
		file.seek_cur(-e.size & 7);
		return r;
	}
}


ISO::TypeArray *MakeArrayType2(const range<uint32*> &dims, const ISO::Type *subtype) {
	if (dims.size() == 1)
		return new ISO::TypeArray(subtype, dims[0]);

	return new ISO::TypeArray(MakeArrayType2(dims.slice_to(-1), subtype), dims.back());
}

const ISO::Type *MakeArrayType(range<uint32*> dims, const ISO::Type *subtype) {
	while (!dims.empty() && dims.front() == 1)
		dims = dims.slice(1);

	if (dims.empty())
		return subtype;
	return MakeArrayType2(dims, subtype);
}

template<bool be, typename T> ISO_ptr<void> ReadArray(tag id, range<uint32*> dims, bool complex, istream_ref file) {
	auto	type	= MakeArrayType(dims, ISO::getdef<T>());

	if (complex) {
		auto	type2	= new(2) ISO::TypeComposite();
		type2->Add(type, "r");
		type2->Add(type, "i");
		auto	p		= ISO::MakePtr(type2, id);
		GetBlock<be, T>(file).raw_data().copy_to(type2->get((void*)p, 0));
		GetBlock<be, T>(file).raw_data().copy_to(type2->get((void*)p, 1));
		return p;

	} else {
		auto	p		= ISO::MakePtr(type, id);
		GetBlock<be, T>(file).raw_data().copy_to((T*)p);
		return p;
	}
}

struct MATLABSparse {
	ISO::OpenArray<int>	ir, jc;
	ISO::OpenArray<double>	r;
	template<typename IR, typename JC, typename R> MATLABSparse(IR &&ir, JC &&jc, R &&r) : ir(forward<IR>(ir)), jc(forward<JC>(jc)), r(forward<R>(r)) {}
};
struct MATLABSparseComplex {
	ISO::OpenArray<int>	ir, jc;
	ISO::OpenArray<double>	r, i;
	template<typename IR, typename JC, typename R, typename I> MATLABSparseComplex(IR &&ir, JC &&jc, R &&r, I &&i) : ir(forward<IR>(ir)), jc(forward<JC>(jc)), r(forward<R>(r)), i(forward<I>(i)) {}
};

ISO_DEFUSERCOMPV(MATLABSparse, ir, jc, r);
ISO_DEFUSERCOMPV(MATLABSparseComplex, ir, jc, r, i);


template<bool be> ISO_ptr<void> ReadVariable(istream_ref file, uint64 end) {
	auto	af		= GetBlock<be, uint32>(file);
	auto	dims	= GetBlock<be, uint32>(file);
	auto	name	= GetBlock<be, char>(file);

	ISO_ASSERT(af.size() == 2);
	tag2	id		= count_string(name.begin(), name.size());
	auto	a		= (MATLAB::array_flags*)af.begin();
	bool	complex	= !!(a->flags & MATLAB::COMPLEX);

	switch (a->clss) {
		case MATLAB::CELL_CLASS: {
			auto			p	= ISO::MakePtr(MakeArrayType(dims, ISO::getdef<ISO_ptr<void>>()), id);
			MATLAB::element<be>	e;
			streamptr		tell;
			for (ISO_ptr<void> *pi = p; (tell = file.tell()) < end && file.read(e) && e.get_type() <= MATLAB::MAX_TYPE; ++pi) {
				ISO_ASSERT(e.get_type() == MATLAB::MATRIX);
				*pi = ReadVariable<be>(file, tell + e.total_size());
			}
			return p;
		}
		case MATLAB::STRUCT_CLASS: {
			auto	name_len	= GetBlock<be, uint32>(file)[0];
			auto	names		= GetBlock<be, char>(file);
			int		num_elements_v		= names.size32() / name_len;
			auto	p			= ISO::MakePtr(MakeArrayType(dims, new ISO::TypeArray(ISO::getdef<ISO_ptr<void>>(), num_elements_v)), id);

			for (ISO_ptr<void> *pi = p; file.tell() < end;) {
				for (auto name = names.begin(); name != names.end(); name += name_len, ++pi) {
					auto	tell	= file.tell();
					auto	e		= file.get<MATLAB::element<be>>();
					ISO_ASSERT(e.get_type() == MATLAB::MATRIX);
					*pi = ReadVariable<be>(file, tell + e.total_size());
					pi->SetID(name);
				}
			}
			return p;
		}
		case MATLAB::OBJECT_CLASS: {
			auto	class_name	= GetBlock<be, char>(file);
			auto	name_len	= GetBlock<be, uint32>(file)[0];
			auto	names		= GetBlock<be, char>(file);
			int		num_elements_v		= names.size32() / name_len;
			auto	p			= ISO::MakePtr(MakeArrayType(dims, new ISO::TypeArray(ISO::getdef<ISO_ptr<void>>(), num_elements_v)), id);

			for (ISO_ptr<void> *pi = p; file.tell() < end;) {
				for (auto name = names.begin(); name != names.end(); name += name_len, ++pi) {
					auto	tell	= file.tell();
					auto	e		= file.get<MATLAB::element<be>>();
					ISO_ASSERT(e.get_type() == MATLAB::MATRIX);
					*pi = ReadVariable<be>(file, tell + e.total_size());
					pi->SetID(name);
				}
			}
			return p;
		}

		case MATLAB::CHAR_CLASS:	return ReadArray<be, char>(id, dims, complex, file);

		case MATLAB::SPARSE_CLASS: {
			auto	ir	= GetBlock<be, int>(file);
			auto	jc	= GetBlock<be, int>(file);
			auto	r	= GetBlock<be, double>(file);

			if (complex) {
				auto	i		= GetBlock<be, double>(file);
				return ISO_ptr<MATLABSparseComplex>(id, ir, jc, r, i);
			} else {
				return ISO_ptr<MATLABSparse>(id, ir, jc, r);
			}
		}
		case MATLAB::DOUBLE_CLASS:	return ReadArray<be, double>(id, dims, complex, file);
		case MATLAB::SINGLE_CLASS:	return ReadArray<be, float> (id, dims, complex, file);
		case MATLAB::INT8_CLASS:	return ReadArray<be, int8>  (id, dims, complex, file);
		case MATLAB::UINT8_CLASS:	return ReadArray<be, uint8> (id, dims, complex, file);
		case MATLAB::INT16_CLASS:	return ReadArray<be, int16> (id, dims, complex, file);
		case MATLAB::UINT16_CLASS:	return ReadArray<be, uint16>(id, dims, complex, file);
		case MATLAB::INT32_CLASS:	return ReadArray<be, int32> (id, dims, complex, file);
		case MATLAB::UINT32_CLASS:	return ReadArray<be, uint32>(id, dims, complex, file);
		case MATLAB::INT64_CLASS:	return ReadArray<be, int64> (id, dims, complex, file);
		case MATLAB::UINT64_CLASS:	return ReadArray<be, uint64>(id, dims, complex, file);
		default: break;
	}
	return ISO_NULL;
}


template<bool be>	void ReadVars(anything &a, istream_ref file, uint64 end = ~uint64(0)) {
	MATLAB::element<be>	e;
	streamptr			tell;
	while ((tell = file.tell()) < end && file.read(e)) {
		auto	type	= e.get_type();
		if (type > MATLAB::MAX_TYPE)
			break;

		ISO_ASSERT(type <= MATLAB::MAX_TYPE);
		if (type == MATLAB::MATRIX) {
			a.Append(ReadVariable<be>(file, tell + 8 + e.size));

		} else if (type == MATLAB::COMPRESSED) {
			ReadVars<be>(a, zlib_reader(file).me());
			file.seek(align(tell + e.size + 8, 8));

		} else {
			auto	data = e.data(file);
			if (!e.compressed())
				file.seek(align(tell + data.length() + 8, 8));
			a.Append(MakeBlock<be>(e.get_type(), data));
		}
	}
}

class MATLAB5FileHandler : public MATLAB5RawFileHandler {
	const char*		GetDescription() override { return "MATLAB v5"; }
	const char*		GetExt() override { return "mat"; }


	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		MATLAB::header	h;
		if (!file.read(h) || !h.valid())
			return ISO_NULL;

		ISO_ptr<anything>	p(id);

		if (h.bigendian())
			ReadVars<true>(*p, file);
		else
			ReadVars<false>(*p, file);

		return p;
	}
} matlab5;
