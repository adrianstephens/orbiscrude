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
			RAWVERTS		= 1<<14,	// don't compress vertex data
			INDEX32			= 1<<15,
		};
		float3p					minext;
		float3p					maxext;
		uint32					flags;
		ISO_ptr<iso::technique>	technique;
		ISO_ptr<void>			parameters;

		SubMeshBase()	: flags(0)	{ clear(minext); clear(maxext); }
		int		GetVertsPerPrim() const	{ return ((flags / VERTSPERPRIMM3) & 255) + 3; }
		void	SetVertsPerPrim(int n)	{ flags |= VERTSPERPRIMM3 * (n - 3); }

		int		GetVertsPerPrim2() const {
		#if 1
			return ISO::Browser(ISO::GetPtr(unconst(this)))["indices"][0].Count();
		#else
			return ISO::GetPtr(this).GetType()->SkipUser()->as<ISO::COMPOSITE>()->Find("indices")->type->SubType()->as<ISO::ARRAY>()->Count();
		#endif
		}

		auto	GetExtent()		const	{ return make_interval(position3(minext), position3(maxext)); }
		template<typename T> auto	SetExtent(const interval<T> &ext)	{
			minext	= ext.minimum();
			maxext	= ext.maximum();
		}
	};

	template<typename T, int N> struct SubMeshTN : SubMeshBase {
		typedef array<T,N>		face;
		ISO_ptr<void>			verts;
		ISO_openarray<face>		indices;
		SubMeshTN()												{}
		SubMeshTN(const SubMeshBase &base) : SubMeshBase(base)	{}
		SubMeshTN(const ISO::Type* vert_type, int nverts, int nfaces) {
			verts = MakePtr(new ISO::TypeOpenArray(vert_type));
			NumVerts(nverts);
			NumFaces(nfaces);
		}
		SubMeshTN(const ISO::TypeOpenArray* vert_type, int nverts, int nfaces) {
			verts = MakePtr(vert_type);
			NumVerts(nverts);
			NumFaces(nfaces);
		}

		uint32	NumFaces()	const		{ return indices.Count(); }
		uint32	NumVerts()	const		{ return ((ISO_openarray<void>*)verts)->Count(); }
		auto	VertsType()	const		{ return verts.GetType()->template as<ISO::OPENARRAY>(); }
		auto	VertType()	const		{ return (const ISO::Type*)VertsType()->subtype; }
		auto	VertComposite()	const	{ return VertType()->SkipUser()->template as<ISO::COMPOSITE>(); }
		uint32	VertSize()	const		{ return VertsType()->subsize; }
		char*	VertData()	const		{ return (char*)VertsType()->ReadPtr(verts); }
		void	NumFaces(uint32 n)		{ indices.Resize(n); }
		void	NumVerts(uint32 n)		{ ((ISO_openarray<void>*)verts)->Resize(VertSize(), 4, n); }
		template<typename V> V*	CreateVerts(uint32 n) {
			verts = ISO_ptr<ISO_openarray<V>>(0, n);
			return (V*)VertData();
		}

		template<typename T> bool	_UpdateExtent(const ISO::Type *type) {
			if (type->template SameAs<T>(ISO::MATCH_COMPOSITE_EXTRA)) {
				SetExtent(get_extent(VertComponentRange<T>(0)));
				return true;
			}
			return false;
		}

		bool	UpdateExtent() {
			auto	pos_type = (*VertComposite())[0].type;
			return _UpdateExtent<double3p>(pos_type)
				|| _UpdateExtent<float3p>(pos_type)
				|| _UpdateExtent<packed_vec<int,3>>(pos_type)
				|| _UpdateExtent<packed_vec<int16,3>>(pos_type)
				|| _UpdateExtent<packed_vec<uint16,3>>(pos_type);
		}

		stride_iterator<void>	_VertComponentData(uint32 offset) const { return stride_iterator<void>(VertData() + offset, VertSize()); }
		auto	VertDataIterator()				const { return _VertComponentData(0); }
		auto	VertDataRange()					const { return make_range_n(_VertComponentData(0), NumVerts()); }
		auto	VertComponents() 				const { return VertComposite()->Components(); }
		auto	VertComponent(uint32 i)			const { return &(*VertComposite())[i]; }
		auto	VertComponent(tag2 id)			const { return VertComposite()->Find(id); }
		ISO::Element*	VertComponent(USAGE2 usage)	const {
			auto	comp = VertComposite();
			for (auto &e : *comp) {
				if (USAGE2(comp->GetID(&e)) == usage)
					return &e;
			}
			return nullptr;
		}

		template<typename T> stride_iterator<T> VertComponentData(uint32 i)	const { return _VertComponentData(VertComponent(i)->offset); }
		template<typename T> stride_iterator<T> VertComponentData(tag2 id)	const { return _VertComponentData(VertComponent(id)->offset); }
		
		template<typename T> auto VertComponentRange(uint32 i)	const { return make_range_n(VertComponentData<T>(i), NumVerts()); }
		template<typename T> auto VertComponentRange(tag2 id)	const { return make_range_n(VertComponentData<T>(id), NumVerts()); }

		auto	VertBlock()						const { return make_block(VertData(), VertSize(), NumVerts()); }
		auto	VertComponentBlock(uint32 offset, uint32 size)	const { return make_strided_block(VertData() + offset, size, VertSize(), NumVerts()); }
		auto	VertComponentBlock(const ISO::Element *e)		const { return VertComponentBlock(e->offset, e->size); }
		auto	VertComponentBlock(uint32 i)	const { return VertComponentBlock(VertComponent(i)); }
		auto	VertComponentBlock(tag2 id)		const { return VertComponentBlock(VertComponent(id)); }
	};
	
	template<int N> using SubMeshN = SubMeshTN<uint32, N>;

	typedef SubMeshTN<uint16, 3> SubMesh16;
	typedef SubMeshTN<uint32, 3> SubMesh;

	struct SubMeshPtr : public ISO_ptr<SubMeshBase> {
		SubMeshPtr()	{}
		using ISO_ptr<SubMeshBase>::operator=;
		using ISO_ptr<SubMeshBase>::ISO_ptr;
		//SubMeshPtr(const ISO_ptr<SubMeshBase> &p)	: ISO_ptr<SubMeshBase>(p)	{}
		//void operator=(const ISO_ptr<SubMeshBase> &p)	{ ISO_ptr<SubMeshBase>::operator=(p); }
	};

	struct Model {
		float3p						minext;
		float3p						maxext;
		ISO_openarray<SubMeshPtr>	submeshes;

		void UpdateExtents() {
			auto	ext = get_extent(transformc(submeshes, [](SubMeshBase *submesh) { return submesh->GetExtent(); }));
			minext	= ext.minimum().v;
			maxext	= ext.maximum().v;
		}
	};
	void Init(Model*,void*);
	void DeInit(Model*);

	struct Model3 {
		float3p						minext;
		float3p						maxext;
		ISO_openarray<SubMeshPtr>	submeshes;

		void UpdateExtents() {
			auto	ext = get_extent(transformc(submeshes, [](SubMeshBase *submesh) { return submesh->GetExtent(); }));
			minext	= ext.minimum().v;
			maxext	= ext.maximum().v;
		}
	};
	void Init(Model3*,void*);
	void DeInit(Model3*);

}
namespace ISO {
	ISO_DEFUSERCOMPV(SubMeshBase, minext, maxext, flags, technique, parameters);

	template<typename T, int N> struct ISO::def<SubMeshTN<T, N>> : ISO::TypeUserCompN<3> {
		def() : ISO::TypeUserCompN<3>(format_string("SubMesh%i", N), WRITETOBIN, &comp) {
			typedef SubMeshTN<T, N> _S, _T;
			ISO_SETBASE(0, SubMeshBase);
			ISO_SETFIELDS(1, verts, indices); 
		};
	};


	ISO_DEFUSERCOMPX(SubMesh16, 3, "SubMesh") {
		ISO_SETBASE(0, SubMeshBase);
		ISO_SETFIELDS2(1, verts, indices);
	}};

	ISO_DEFUSER(SubMeshPtr,			ISO_ptr<SubMeshBase>);

	template<> struct ISO::def<Model3> : ISO::TypeUserCallback {
		ISO::TypeCompositeN<3>	comp;
		def() : ISO::TypeUserCallback((Model3*)0, "Model3", &comp, CHANGE | WRITETOBIN) {
			typedef Model3	_S, _T;
			ISO::Element	*fields = comp.fields;
			ISO_SETFIELDS3(0, minext, maxext, submeshes);
		}
	};

	template<> struct ISO::def<Model> : ISO::TypeUserCallback {
		ISO::TypeCompositeN<3>	comp;
		def() : ISO::TypeUserCallback((Model*)0, "Model", &comp, CHANGE | WRITETOBIN) {
			typedef Model	_S, _T;
			ISO::Element	*fields = comp.fields;
			ISO_SETFIELDS3(0, minext, maxext, submeshes);
		}
	};

} // namespace ISO
#endif	// MODEL_ISO_H
