#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "filetypes/3d/model_utils.h"
#include "codec/mesh/draco.h"
#include "utilities.h"

using namespace iso;

ISO_ptr<void> MakeISO(const draco::Attribute& a) {
	switch (a.num_components) {
		default:return ISO_ptr<ISO_openarray<int>>(0, a.values);
		case 2:	return ISO_ptr<ISO_openarray<array<int, 2>>>(0, a.values);
		case 3:	return ISO_ptr<ISO_openarray<array<int, 3>>>(0, a.values);
		case 4:	return ISO_ptr<ISO_openarray<array<int, 4>>>(0, a.values);
	}
}

const ISO::Type* GetType(const draco::Attribute& a) {
	static const ISO::Type* types[] = {
		0,						//DT_INVALID
		ISO::getdef<int8>(),	//DT_INT8,
		ISO::getdef<uint8>(),	//DT_UINT8,
		ISO::getdef<int16>(),	//DT_INT16,
		ISO::getdef<uint16>(),	//DT_UINT16,
		ISO::getdef<int32>(),	//DT_INT32,
		ISO::getdef<uint32>(),	//DT_UINT32,
		ISO::getdef<int64>(),	//DT_INT64,
		ISO::getdef<uint64>(),	//DT_UINT64,
		ISO::getdef<float32>(),	//DT_FLOAT32,
		ISO::getdef<float64>(),	//DT_FLOAT64,
		ISO::getdef<bool>(),	//DT_BOOL,
	};
	auto	type = types[a.data_type];
	return a.num_components > 1 ? new ISO::TypeArray(type, a.num_components) : type;
}

tag GetID(const draco::Attribute& a, uint32 &flags) {
	switch (a.type) {
		default:	return "unknown";
		case draco::Attribute::POSITION:	flags |= 1; return "position";
		case draco::Attribute::NORMAL:		flags |= 2; return "normal";
		case draco::Attribute::COLOR:		flags |= 4; return "color";
		case draco::Attribute::TEX_COORD:	flags |= 8; return "texcoord";
		case draco::Attribute::GENERIC:		return "generic";
	}
}

const draco::DataType GetDataType(const ISO::Type* type, int& num_components) {
	type = type->SkipUser();
	if (type->GetType() == ISO::ARRAY) {
		num_components = ((ISO::TypeArray*)type)->count;
		type = (((ISO::TypeArray*)type)->subtype)->SkipUser();
	}
	switch (type->GetType()) {
		case ISO::INT: {
			auto	i = (const ISO::TypeInt*)type;
			bool	sign = i->is_signed();
			switch (i->GetSize()) {
				case 1: return sign ? draco::DT_INT8  : draco::DT_UINT8;
				case 2: return sign ? draco::DT_INT16 : draco::DT_UINT16;
				case 4: return sign ? draco::DT_INT32 : draco::DT_UINT32;
				case 8: return sign ? draco::DT_INT64 : draco::DT_UINT64;
				default: break;
			}
			break;
		}
		case ISO::FLOAT:
			switch (((const ISO::TypeFloat*)type)->GetSize()) {
				case 4:	return draco::DT_FLOAT32;
				case 8:	return draco::DT_FLOAT64;
				default: break;
			}
			break;
	}
	return draco::DT_INVALID;
}

const draco::Attribute::Type GetType(USAGE2 u) {
	switch (u.usage) {
		case USAGE_POSITION:	return draco::Attribute::POSITION;
		case USAGE_TANGENT:
		case USAGE_BINORMAL:
		case USAGE_NORMAL:		return draco::Attribute::NORMAL;
		case USAGE_COLOR:		return draco::Attribute::COLOR;
		case USAGE_TEXCOORD:	return draco::Attribute::TEX_COORD;
		default:				return draco::Attribute::GENERIC;
	}
}

class DRACOFileHandler : FileHandler {
	const char* GetExt()			override { return "draco"; }
	const char*	GetDescription()	override { return "Draco 3D data compression";	}

#ifdef DRACO_ENABLE_READER
	int			Check(istream_ref file) override {
		file.seek(0);
		draco::Header	h;
		return file.read(h) && h.valid() ? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		draco::Reader	dr;
		if (!dr.read(file))
			return ISO_NULL;

		ISO_ptr<Model3>			model(id);

		ISO::TypeCompositeN<64>	builder(0);
		uint32	flags = 0;
		for (auto& d : dr.dec) {
			for (auto& a : d.attributes)
				builder.Add(GetType(a), GetID(a, flags));
		}

		auto	num_verts	= dr.NumPoints();
		auto	num_faces	= dr.NumFaces();

		ISO_ptr<SubMesh>	mesh(0, builder.Duplicate(), num_verts, num_faces ? num_faces : num_verts / 3);
		model->submeshes.Append(mesh);

		mesh->technique = flags & 2 ? ISO::root("data")["default"]["specular"]
			: flags & 4 ? ISO::root("data")["default"]["col_vc"]
			: ISO::root("data")["default"]["coloured"];

		if (num_faces) {
			// for mesh
			int32	*i = dr.CornerToPoint();
			for (auto& f : mesh->indices) {
				f[0] = i[0];
				f[1] = i[1];
				f[2] = i[2];
				i += 3;
			}

		} else {
			// for point cloud
			int	i = 0;
			for (auto& f : mesh->indices) {
				f[0] = i++;
				f[1] = i++;
				f[2] = i++;
			}
		}

		int		c = 0;
		for (auto& d : dr.dec) {
			auto	point_to_value = dr.PointToValue(d);
			for (auto& a : d.attributes) {
				a.CopyValues(mesh->VertComponentData<int8>(c), point_to_value);
				++c;
			}
		}

		mesh->UpdateExtents();
		model->UpdateExtents();
		
		return model;
	}
#endif

#ifdef DRACO_ENABLE_WRITER
	bool	Write(ISO::ptr<void> p, iso::ostream_ref file) override {
		ISO_ptr<Model3> model = ISO_conversion::convert<Model3>(p, ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE);
		if (!model)
			return false;

		draco::Writer	dw(DefaultMode(draco::TRIANGULAR_MESH_EDGEBREAKER, ISO::root("variables")["compression"].GetInt(75) / 10));// | draco::USE_SINGLE_CONNECTIVITY);
		for (SubMesh *mesh : model->submeshes) {
			dw.SetIndices(make_range_n(&mesh->indices[0][0], mesh->indices.size() * 3));
			
			auto	num_verts	= mesh->NumVerts();
			int		id			= -1;
			for (auto &&e : mesh->VertComponents()) {
				int		num_components;
				auto	data_type	= GetDataType(e.type, num_components);
				auto	type		= GetType(USAGE2(e.id));
				auto	a			= dw.AddAttribute(id++, type, data_type, num_components, false);

				temp_block	decoded(e.size * num_verts);
				uint8		*dst	= decoded;
				for (auto&& src : mesh->VertComponentBlock(e.offset, e.size)) {
					memcpy(dst, src, e.size);
					dst += e.size;
				}
				a->SetValues(decoded);
			}
		}
		return dw.write(file);
	}
#endif


} draco_handler;

class DRCFileHandler : DRACOFileHandler {
	const char* GetExt()		override { return "drc"; }
};

