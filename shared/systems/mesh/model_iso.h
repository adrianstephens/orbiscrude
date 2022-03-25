#ifndef MODEL_ISO_H
#define MODEL_ISO_H

#include "scenegraph.h"

namespace iso {
	struct SubMeshBase {
		enum {
			SORT			= 1<<0,
			NOSHADOW		= 1<<1,
			USETEXTURE		= 1<<2,
			UPPERSHADOW		= 1<<3,
			MIDDLESHADOW	= 1<<4, DRAWFIRST = MIDDLESHADOW,
			DRAWLAST		= 1<<5,
			DOUBLESIDED		= 1<<6, EDGES = DOUBLESIDED,
			COLLISION		= 1<<7,
			STRIP			= 1<<8,
			ADJACENCY		= 1<<9,
			VERTSPERPRIMM3	= 1<<10,
		};
		float3p						minext;
		float3p						maxext;
		uint32						flags;
		ISO_ptr<iso::technique>		technique;
		ISO_ptr<void>				parameters;
		SubMeshBase()	: flags(0)	{ clear(minext); clear(maxext); }
		int		GetVertsPerPrim() const	{ return ((flags / VERTSPERPRIMM3) & 255) + 3;			}
		void	SetVertsPerPrim(int n)	{ flags |= VERTSPERPRIMM3 * (n - 3);					}
	};

	template<int N> struct SubMeshN : SubMeshBase {
		typedef array<uint16,N>	face;
		ISO_ptr<void>					verts;
		ISO_openarray<face>				indices;
		SubMeshN()												{}
		SubMeshN(const SubMeshBase &base) : SubMeshBase(base)	{}
		SubMeshN(const ISO::Type* vert_type, int nverts, int nfaces) {
			verts = MakePtr(new ISO::TypeOpenArray(vert_type));
			NumVerts(nverts);
			NumFaces(nfaces);
		}
		SubMeshN(const ISO::TypeOpenArray* vert_type, int nverts, int nfaces) {
			verts = MakePtr(vert_type);
			NumVerts(nverts);
			NumFaces(nfaces);
		}

		uint32	NumFaces()	const		{ return indices.Count();								}
		uint32	NumVerts()	const		{ return ((ISO_openarray<void>*)verts)->Count();		}
		auto	VertType()	const		{ return (const ISO::Type*)((ISO::TypeOpenArray*)verts.GetType())->subtype;	}
		auto	VertComposite()	const	{ return (ISO::TypeComposite*)VertType()->SkipUser(); }
		uint32	VertSize()	const		{ return ((ISO::TypeOpenArray*)verts.GetType())->subsize;	}
		char*	VertData()	const		{ return (char*)((ISO::TypeOpenArray*)verts.GetType())->ReadPtr(verts); }
		void	NumFaces(uint32 n)		{ indices.Resize(n);									}
		void	NumVerts(uint32 n)		{ ((ISO_openarray<void>*)verts)->Resize(VertSize(), 4, n); }
		template<typename V> V*	CreateVerts(uint32 n)		{
			verts = ISO_ptr<ISO_openarray<V>>(0, n);
			return (V*)VertData();
		}

		void	UpdateExtents() {
			if (VertType()->template Is<double3p>()) {
				auto	ext = get_extent(VertComponentRange<double3p>(0));
				minext	= ext.minimum();
				maxext	= ext.maximum();
			} else {
				auto	ext = get_extent(VertComponentRange<float3p>(0));
				minext	= ext.minimum();
				maxext	= ext.maximum();
			}
		}



		stride_iterator<void>	_VertComponentData(uint32 offset) const { return stride_iterator<void>(VertData() + offset, VertSize()); }
		auto	VertDataIterator()				const { return _VertComponentData(0); }
		auto	VertDataRange()					const { return make_range_n(_VertComponentData(0), NumVerts()); }
		auto	VertComponents() 				const { return VertComposite()->Components(); }
		auto	VertComponent(uint32 i)			const { return &(*VertComposite())[i]; }
		auto	VertComponent(tag2 id)			const { return VertComposite()->Find(id); }

		template<typename T> stride_iterator<T> VertComponentData(uint32 i)	const { return _VertComponentData(VertComponent(i)->offset); }
		template<typename T> stride_iterator<T> VertComponentData(tag2 id)	const { return _VertComponentData(VertComponent(id)->offset); }
		
		template<typename T> auto VertComponentRange(uint32 i)	const { return make_range_n(VertComponentData<T>(i), NumVerts()); }
		template<typename T> auto VertComponentRange(tag2 id)	const { return make_range_n(VertComponentData<T>(id), NumVerts()); }

		auto	VertComponentBlock(uint32 offset, uint32 size)	const { return make_strided_block(VertData() + offset, size, VertSize(), NumVerts()); }
		auto	VertComponentBlock(const ISO::Element *e)		const { return VertComponentBlock(e->offset, e->size); }
		auto	VertComponentBlock(uint32 i)	const { return VertComponentBlock(VertComponent(i)); }
		auto	VertComponentBlock(tag2 id)		const { return VertComponentBlock(VertComponent(id)); }
	};

	typedef SubMeshN<3> SubMesh;

	struct SubMeshPtr : public ISO_ptr<SubMeshBase> {
		SubMeshPtr()	{}
		SubMeshPtr(const ISO_ptr<SubMeshBase> &p)	: ISO_ptr<SubMeshBase>(p)	{}
		void operator=(const ISO_ptr<SubMeshBase> &p)	{ *(ISO_ptr<SubMeshBase>*)this = p; }
	};

	struct Model3 {
		float3p						minext;
		float3p						maxext;
		ISO_openarray<SubMeshPtr>	submeshes;

		void UpdateExtents() {
			auto	ext = get_extent(transformc(submeshes, [](SubMeshBase *submesh) { return make_interval(position3(submesh->minext), position3(submesh->maxext)); }));
			minext	= ext.minimum().v;
			maxext	= ext.maximum().v;
		}
	};
	void Init(Model3*,void*);
	void DeInit(Model3*);

}
namespace ISO {
	ISO_DEFUSERCOMPV(SubMeshBase, minext, maxext, flags, technique, parameters);

	template<int N> ISO_DEFUSERCOMPTX(SubMeshN, N, 3, "SubMesh") {
		ISO_SETBASE(0, SubMeshBase);
		ISO_SETFIELDS2(1, verts, indices);
	}};

//	ISO_DEFUSER(SubMeshPtr,			ISO_ptr<void>);
	ISO_DEFUSER(SubMeshPtr,			ISO_ptr<SubMeshBase>);

	template<> struct ISO::def<Model3> : ISO::TypeUserCallback {
		ISO::TypeCompositeN<3>	comp;
		def() : ISO::TypeUserCallback((Model3*)0, "Model3", &comp, CHANGE | WRITETOBIN) {
			typedef Model3	_S, _T;
			ISO::Element	*fields = comp.fields;
			ISO_SETFIELDS3(0, minext, maxext, submeshes);
		}
	};
} // namespace ISO
#endif	// MODEL_ISO_H
