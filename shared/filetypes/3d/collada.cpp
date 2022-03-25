#include "iso/iso_files.h"
#include "extra/xml.h"
#include "extra/indexer.h"
#include "base/hash.h"
#include "systems/mesh/model_iso.h"

using namespace iso;

struct COLLADA {
	XMLreader::Data	data;
	XMLreader				xml;
	ISO_ptr<anything>		p;

	struct library_images;
	struct library_materials;
	struct library_effects;
	struct library_animation_clips;
	struct library_animations;
	struct library_cameras;
	struct library_controllers;
	struct library_geometries;
	struct library_lights;
	struct library_nodes;
	struct library_visual_scenes;

	typedef	void	(*ftype)(COLLADA*);
	static struct fn_table {
		hash_map<crc32, ftype, false, 8>	h;
		fn_table();
	} table;

	template<typename T> void process();
	template<typename T> static void static_process(COLLADA *p) { p->process<T>(); }

	COLLADA(tag id, istream_ref file) : xml(file), p(id) {
	}

	ISO_ptr<void> Read();
};

ISO_ptr<void> COLLADA::Read() {
	if (xml.ReadNext(data, XMLreader::TAG_BEGIN) != XMLreader::TAG_BEGIN || !data.Is("COLLADA"))
		return ISO_NULL;

	try {
		while (xml.ReadNext(data, XMLreader::TAG_BEGIN) == XMLreader::TAG_BEGIN) {
			if (auto f = table.h.check(data.Name()))
				(*f)(this);
			xml.ReadNext(data, XMLreader::TAG_END);
		}
		return p;

	} catch (const char *error) {
		throw_accum(error << " at line " << xml.GetLineNumber());
		return ISO_NULL;
	}
}

template<> void COLLADA::process<COLLADA::library_images>() 			{}
template<> void COLLADA::process<COLLADA::library_materials>() 			{}
template<> void COLLADA::process<COLLADA::library_effects>() 			{}
template<> void COLLADA::process<COLLADA::library_animation_clips>()	{}
template<> void COLLADA::process<COLLADA::library_animations>() 		{}
template<> void COLLADA::process<COLLADA::library_cameras>() 			{}
template<> void COLLADA::process<COLLADA::library_controllers>() 		{}
template<> void COLLADA::process<COLLADA::library_lights>() 			{}
template<> void COLLADA::process<COLLADA::library_nodes>() 				{}
template<> void COLLADA::process<COLLADA::library_visual_scenes>() 		{}

template<> void COLLADA::process<COLLADA::library_geometries>() {
	XMLiterator	it(xml, data);

	while (it.Next()) {
		if (data.Is("geometry")) {
			ISO_ptr<Model3>	model(data.Find("id"));
			p->Append(model);

			it.Enter();
			while (it.Next()) {
				if (data.Is("mesh")) {
					ISO_ptr<SubMesh>	submesh(data.Find("id"));
					model->submeshes.Append(submesh);

					struct Stream {
						string	name;
						string	id;
						string	semantic;
						uint32	count;
						uint32	stride;
						uint32	offset;
						float	*data;
						bool	operator==(const char *s) const { return id == s; }
					};
					static_array<Stream, 64>	streams;
					uint8				*vcounts;
					uint32				*indices;
					uint32				nprims, nindices = 0, num_offsets = 0;
					enum PRIM {none, lines, linestrips, polygons, polylist, triangles, trifans, tristrips } prim;

					it.Enter();
					while (it.Next()) {
						if (data.Is("source")) {
							Stream	&stream	= streams.push_back();
							stream.name		= data.Find("name");
							stream.id		= data.Find("id");
							stream.offset	= -1;

							it.Enter();
							while (it.Next()) {
								if (data.Is("float_array")) {
									string_scan(data.Find("count")) >> stream.count;
									xml.ReadNext(data, XMLreader::TAG_CONTENT);
									string_scan	ss(data.Content());
									stream.data	= new float[stream.count];
									for (int i = 0; i < stream.count; i++)
										ss >> stream.data[i];

								} else if (data.Is("technique_common")) {
									if (xml.ReadNext(data, XMLreader::TAG_BEGIN) == XMLreader::TAG_BEGIN && data.Is("accessor"))
										string_scan(data.Find("stride")) >> stream.stride;
									it.Next();
								}
							}

						} else if (data.Is("vertices")) {
							it.Enter();
							while (it.Next()) {
								if (data.Is("input")) {
									Stream	*s	= find(streams, data.Find("source") + 1);
									s->semantic	= data.Find("semantic");
									s->offset	= 0;
								}
							}

						} else if (prim
							= data.Is("lines")			? lines
							: data.Is("linestrips")	? linestrips
							: data.Is("polygons")		? polygons
							: data.Is("polylist")		? polylist
							: data.Is("triangles")		? triangles
							: data.Is("trifans")		? trifans
							: data.Is("tristrips")		? tristrips
							: none
						) {
							string_scan(data.Find("count")) >> nprims;
							switch (prim) {
								case polylist:	vcounts = new uint8[nprims]; break;
								case triangles:	nindices = nprims * 3; break;
								case trifans:
								case tristrips:	nindices = nprims + 2; break;
							}
							it.Enter();
							while (it.Next()) {
								if (data.Is("input")) {
									uint32	off	= string_scan(data.Find("offset")).get();
									num_offsets	= max(num_offsets, off + 1);

									if (data.Find("semantic") == "VERTEX") {
									} else {
										Stream	*s	= find(streams, data.Find("source") + 1);
										s->semantic	= data.Find("semantic");
										s->offset	= off;
									}

								} else if (prim == polylist && data.Is("vcount")) {
									xml.ReadNext(data, XMLreader::TAG_CONTENT);
									string_scan	ss(data.Content());
									for (int i = 0; i < nprims; i++) {
										ss >> vcounts[i];
										nindices += vcounts[i];
									}

								} else if (data.Is("p")) {
									xml.ReadNext(data, XMLreader::TAG_CONTENT);
									uint32	n	= nindices * num_offsets;
									indices		= new uint32[n];
									string_scan	ss(data.Content());
									for (int i = 0; i < n; i++)
										ss >> indices[i];
								}
							}
						}
					}

					ISO::TypeCompositeN<64>	comp(0);
					for (auto &i : streams) {
						static const ISO::Type* types[] = {0, ISO::getdef<float>(),  ISO::getdef<float[2]>(),  ISO::getdef<float[3]>(),  ISO::getdef<float[4]>()};
						comp.Add(types[i.stride], i.name);
					}

					Indexer<uint32>	indexer(nindices);
					uint32			*temp		= new uint32[nindices];

					for (int o = 0; o < num_offsets; o++) {
						for (int i = 0; i < nindices; i++)
							temp[i] = indices[i * num_offsets + o];
						if (o == 0)
							indexer.ProcessFirst(temp, equal_to());//, keygen<uint32>());
						else
							indexer.Process(temp, equal_to());
					}

					const uint32	*ix		= indexer.Indices().begin();
					switch (prim) {
						case polylist: {
							uint32	ntris = 0;
							for (int i = 0; i < nprims; i++)
								ntris += vcounts[i] - 2;
							SubMesh::face *tris = submesh->indices.Create(ntris, false);
							for (int i = 0; i < nprims; i++) {
								for (const uint32 *ix0 = ix, *ix1 = ix += vcounts[i]; ix0 < ix1 - 2; tris += 2) {
									tris[0][0]	= *ix0++;
									tris[0][1]	= *ix0;
									tris[0][2]	= *--ix1;
									if (ix0 == ix1 - 1)
										break;

									tris[1][0]	= *ix0;
									tris[1][1]	= ix1[-1];
									tris[1][2]	= *ix1;
								}
							}
							break;
						}

						case triangles:
							for (SubMesh::face *tris = submesh->indices.Create(nprims, false), *end = tris + nprims; tris < end; ++tris) {
								tris[0][0]	= *ix++;
								tris[0][1]	= *ix++;
								tris[0][2]	= *ix++;
							}
							break;

						case tristrips:
							for (SubMesh::face *tris = submesh->indices.Create(nprims, false), *end = tris + nprims; tris < end; ix += 2, tris += 2) {
								tris[0][0]	= ix[0];
								tris[0][1]	= ix[1];
								tris[0][2]	= ix[2];
								if (tris == end)
									break;
								tris[1][0]	= ix[2];
								tris[1][1]	= ix[1];
								tris[1][2]	= ix[3];
							}
							break;

						case trifans: {
							const uint32	*ixp = ix + 2;
							for (SubMesh::face *tris = submesh->indices.Create(nprims, false), *end = tris + nprims; tris < end; ++tris) {
								tris[0][0]	= ix[0];
								tris[0][1]	= ix[1];
								tris[0][2]	= *ixp++;
							}
							break;
						}
					}

					uint32			nverts	= indexer.NumUnique();
					uint32			stride	= comp.GetSize();
					ISO_openarray<void>	data(stride, nverts);
					uint8			*dest	= (uint8*)(void*)data, *dend = dest + stride * nverts;

					size_t			nstreams	= streams.size();
					for (int s = 0; s < nstreams; s++) {
						uint8	*srce	= (uint8*)streams[s].data;
						uint32	size	= streams[s].stride * sizeof(float);
						uint32	*ix		= indices + streams[s].offset;
						for (int i = 0; i < nverts; i++) {
							uint32	x = indexer.RevIndex(i);
							uint32	y = ix[x * num_offsets];
							memcpy(dest + i * stride, srce + y * size, size);
						}
						dest += size;
					}

					submesh->verts = MakePtr(new ISO::TypeOpenArray(comp.Duplicate()));
					*(ISO_openarray<void>*)submesh->verts = data;
					submesh->technique = ISO::root("data")["simple"]["lite"];

					clear(submesh->minext);
					clear(submesh->maxext);
					float	*stream0 = streams[0].data, *stream0end = stream0 + streams[0].count;
					uint32	stream0stride	= streams[0].stride;
					for (int c = 0; c < stream0stride; c++) {
						float	a = stream0[c], b = a;
						for (float *p = stream0 + c; p < stream0end; p += stream0stride) {
							a = min(a, *p);
							b = max(b, *p);
						}
						submesh->minext[c] = a;
						submesh->maxext[c] = b;
					}
				}
			}

			float3	minext1, maxext1;
			for (int i = 0, n = model->submeshes.Count(); i < n; i++) {
				SubMeshBase		*submesh	= model->submeshes[i];
				if (i == 0) {
					minext1 = submesh->minext;
					maxext1 = submesh->maxext;
				} else {
					minext1 = min(minext1, submesh->minext);
					maxext1 = max(maxext1, submesh->maxext);
				}
			}
			model->minext = minext1;
			model->maxext = maxext1;
		}
	}
}

COLLADA::fn_table COLLADA::table;
COLLADA::fn_table::fn_table() {
	h["library_images"]				= COLLADA::static_process<COLLADA::library_images>;
	h["library_materials"]			= COLLADA::static_process<COLLADA::library_materials>;
	h["library_effects"]			= COLLADA::static_process<COLLADA::library_effects>;
	h["library_animation_clips"]	= COLLADA::static_process<COLLADA::library_animation_clips>;
	h["library_animations"]			= COLLADA::static_process<COLLADA::library_animations>;
	h["library_cameras"]			= COLLADA::static_process<COLLADA::library_cameras>;
	h["library_controllers"]		= COLLADA::static_process<COLLADA::library_controllers>;
	h["library_geometries"]			= COLLADA::static_process<COLLADA::library_geometries>;
	h["library_lights"]				= COLLADA::static_process<COLLADA::library_lights>;
	h["library_nodes"]				= COLLADA::static_process<COLLADA::library_nodes>;
	h["library_visual_scenes"]		= COLLADA::static_process<COLLADA::library_visual_scenes>;
}

class ColladaFileHandler : public FileHandler {
	const char*		GetExt() override { return "dae"; }
	const char*		GetDescription() override { return "Collada";	}
	int				Check(istream_ref file) override {
		file.seek(0);
		XMLreader	xml(file);
		XMLreader::Data	data;
		return xml.CheckVersion()			>= 0
			&& xml.ReadNext(data)			== XMLreader::TAG_BEGIN
			&& data.Is("COLLADA")
			&& data.Find("xmlns")	== "http://www.collada.org/2005/11/COLLADASchema"
			? CHECK_PROBABLE : CHECK_DEFINITE_NO;
	}

	ISO_ptr<void>	Read(tag id, istream_ref file) override {
		COLLADA	collada(id, file);
		return collada.Read();
	}
} collada;

#if 0
//Animation
animation				//Categorizes the declaration of animation information.
animation_clip			//Defines a section of the animation curves to be used together as an animation clip.
channel					//Declares an output channel of an animation.
instance_animation		//Declares the instantiation of a COLLADA animation resource.
library_animation_clips	//Declares a module of <animation_clip> elements.
library_animations		//Declares a module of <animation> elements.
sampler					//Declares an interpolation sampling function for an animation.

//Camera
camera					//Declares a view into the scene hierarchy or scene graph. The camera contains elements that describe the camera’s optics and imager.
imager					//Represents the image sensor of a camera (for example, film or CCD).
instance_camera			//Declares the instantiation of a COLLADA camera resource.
library_cameras			//Declares a module of <camera> elements.
optics					//Represents the apparatus on a camera that projects the image onto the image sensor.
orthographic			//Describes the field of view of an orthographic camera.
perspective				//Describes the field of view of a perspective camera.

//Controller
controller				//Categorizes the declaration of generic control information.
instance_controller		//Declares the instantiation of a COLLADA controller resource.
joints					//Declares the association between joint nodes and attribute data.
library_controllers		//Declares a module of <controller> elements.
morph					//Describes the data required to blend between sets of static meshes.
skeleton				//Indicates where a skin controller is to start to search for the joint nodes that it needs.
skin					//Contains vertex and primitive information sufficient to describe blend-weight skinning.
targets					//Declares morph targets, their weights, and any user-defined attributes associated with them.
vertex_weights			//Describes the combination of joints and weights used by a skin.

//Data Flow
accessor				//Declares an access pattern to one of the array elements <float_array>,<int_array>, <Name_array>, <bool_array>, and <IDREF_array>.
bool_array				//Declares the storage for a homogenous array of Boolean values.
float_array				//Declares the storage for a homogenous array of floating-point values.
IDREF_array				//Declares the storage for a homogenous array of ID reference values.
int_array				//Stores a homogenous array of integer values.
Name_array				//Stores a homogenous array of symbolic name values.
param (core)			//Declares parametric information for its parent element.
source					//Declares a data repository that provides values according to the semantics of an <input> element that refers to it.
input (indexed)			//Declares the input semantics of a data source.
input (unindexed)		//Declares the input semantics of a data source.

//Extensibility
extra					//Provides arbitrary additional information about or related to its parent element.
technique (core)		//Declares the information used to process some portion of the content. Each technique conforms to an associated profile.
technique_common		//Specifies the information for a specific element for the common profile that all COLLADA implementations must support.

//Geometry
control_vertices		//Describes the control vertices (CVs) of a spline.
geometry				//Describes the visual shape and appearance of an object in a scene.
instance_geometry		//Declares the instantiation of a COLLADA geometry resource.
library_geometries		//Declares a module of <geometry> elements.
lines					//Declares the binding of geometric primitives and vertex attributes for a <mesh> element.
linestrips				//Declares a binding of geometric primitives and vertex attributes for a <mesh> element.
mesh					//Describes basic geometric meshes using vertex and primitive information.
polygons				//Declares the binding of geometric primitives and vertex attributes for a <mesh> element.
polylist				//Declares the binding of geometric primitives and vertex attributes for a <mesh> element.
spline					//Describes a multisegment spline with control vertex (CV) and segment information.
triangles				//Declares the binding of geometric primitives and vertex attributes for a <mesh> element. (or) Provides the information needed to bind vertex attributes together and then organize those vertices into individual triangles.
trifans					//Declares the binding of geometric primitives and vertex attributes for a <mesh> element. (or) Provides the information needed to bind vertex attributes together and then organize those vertices into connected triangles
tristrips				//Declares the binding of geometric primitives and vertex attributes for a <mesh> element. (or) Provides the information needed to bind vertex attributes together and then organize those vertices into connected triangles
vertices				//Declares the attributes and identity of mesh-vertices.

//Lighting
ambient (core)			//Describes an ambient light source.
color					//Describes the color of its parent light element.
directional				//Describes a directional light source.
instance_light			//Declares the instantiation of a COLLADA light resource.
library_lights			//Declares a module of <image> elements.
light					//Declares a light source that illuminates a scene.
point					//Describes a point light source.
spot					//Describes a spot light source.

//Metadata
asset					//Defines asset-management information regarding its parent element.
COLLADA					//Declares the root of the document that contains some of the content in the COLLADA schema.
contributor				//Defines authoring information for asset management.

//Scene
instance_node			//Declares the instantiation of a COLLADA node resource.
instance_visual_scene	//Declares the instantiation of a COLLADA visual_scene resource.
library_nodes			//Declares a module of <node> elements.
library_visual_scenes	//Declares a module of <visual_scene> elements.
node					//Embodies the hierarchical relationship of elements in a scene.
scene					//Embodies the entire set of information that can be visualized from the contents of a COLLADA resource.
visual_scene			//Embodies the entire set of information that can be visualized from the contents of a COLLADA resource.

//Transform
lookat					//Contains a position and orientation transformation suitable for aiming a camera.
matrix					//Describes transformations that embody mathematical changes to points within a coordinate system or the coordinate system itself.
rotate					//Specifies how to rotate an object around an axis.
scale					//Specifies how to change an object’s size.
skew					//Specifies how to deform an object along one axis.
translate				//Changes the position of an object in a coordinate system without any rotation.
#endif

