#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "systems/mesh/model_iso.h"
#include "scenegraph.h"

using namespace iso;

class DXFFileHandler : FileHandler {
	const char*		GetExt() override { return "dxf";			}
	const char*		GetDescription() override { return "Autocad DXF";	}
	bool			Write(ISO_ptr<void> p, ostream_ref file) override;
} dxf;

bool DXFFileHandler::Write(ISO_ptr<void> p, ostream_ref file) {

	ISO_ptr<Model3> model = ISO_conversion::convert<Model3>(FileHandler::ExpandExternals(p), ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE);
	if (!model)
		return false;

	// Header section
	file.write(
		"0\nSECTION\n2\nHEADER\n"
		"9\n$ACADVER\n1\nAC1008\n"
		"9\n$UCSORG\n10\n0.0\n20\n0.0\n30\n0.0\n"
		"9\n$UCSXDIR\n10\n1.0\n20\n0.0\n30\n0.0\n"
		"9\n$TILEMODE\n70\n1\n"
		"9\n$UCSYDIR\n10\n0.0\n20\n1.0\n30\n0.0\n"
	);
	file.write(format_string("9\n$EXTMIN\n10\n%f\n20\n%f\n30\n%f\n", model->minext.x, model->minext.x, model->minext.x));
	file.write(format_string("9\n$EXTMAX\n10\n%f\n20\n%f\n30\n%f\n", model->maxext.x, model->maxext.x, model->maxext.x));
	file.write("0\nENDSEC\n");

	file.write(
	// Tables section
		"0\nSECTION\n2\nTABLES\n"

	// Continuous line type
		"0\nTABLE\n2\nLTYPE\n70\n1\n0\nLTYPE\n2\nCONTINUOUS\n70\n64\n3\nSolid line\n72\n65\n73\n0\n40\n0.0\n"
		"0\nENDTAB\n"

	// Object names for layers
		"0\nTABLE\n2\nLAYER\n70\n1\n"
		"0\nLAYER\n2\n0\n70\n0\n62\n7\n6\nCONTINUOUS\n"
		"0\nENDTAB\n"

	// Default style
		"0\nTABLE\n2\nSTYLE\n70\n1\n0\nSTYLE\n2\nSTANDARD\n70\n0\n40\n0.0\n41\n1.0\n50\n0.0\n71\n0\n42\n0.2\n3\ntxt\n4\n\n0\nENDTAB\n"

	// Default View?
	// UCS
		"0\nTABLE\n2\nUCS\n70\n0\n0\nENDTAB\n"
		"0\nENDSEC\n"
	);

	// Entities section
	ISO_ptr<SubMesh>	submesh		= (ISO_ptr<SubMesh>&)model->submeshes[0];
	ISO::TypeOpenArray	*vertstype	= (ISO::TypeOpenArray*)submesh->verts.GetType();
	uint32				vertsize	= vertstype->subsize;
	ISO_openarray<char>	*verts		= submesh->verts;
	int					nverts		= verts->Count();
	int					ntris		= submesh->indices.Count();

	file.write("0\nSECTION\n2\nENTITIES\n");
	file.write(format_string("0\nPOLYLINE\n8\n0\n66\n1\n70\n64\n71\n%u\n72\n%u\n", nverts, ntris));
	file.write("62\n7\n");//line colour


	for (int i = 0; i < nverts; i++) {
		float3 v = load<float3>((float*)(verts + vertsize * i));
		v *= 100.f / 2.54f;
//		float3 v = float3((float*)(verts + vertsize * i)) * (100.f / 2.54f);	//get into inches
		file.write(format_string("0\nVERTEX\n8\n0\n10\n%.6f\n20\n%.6f\n30\n%.6f\n70\n192\n", float(v.x), float(v.z), -float(v.y)));
	}

	for (int i = 0; i < ntris; i++) {
		SubMesh::face &f = submesh->indices[i];
		file.write(format_string("0\nVERTEX\n8\n0\n10\n0\n20\n0\n30\n0\n70\n128\n71\n%d\n72\n%d\n73\n%d\n", f[0] + 1, f[1] + 1, f[2] + 1));
	}

	file.write(
		"0\nSEQEND\n8\n0\n"
		"0\nENDSEC\n0\nEOF\n"
	);
	return true;
}
