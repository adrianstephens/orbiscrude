#include "scenegraph.h"
#include "iso/iso_convert.h"
#include "piece_wise.h"
#include "packed_types.h"

using namespace iso;

//-----------------------------------------------------------------------------
//	Animation
//-----------------------------------------------------------------------------

template<typename T> struct CompressedStream : dynamic_array<pair<T, float> > {
	typedef dynamic_array<pair<T, float> >	B;

	template<typename P> CompressedStream(const P *p, size_t n, float eps) : dynamic_array<pair<T, float> >(n) {
		float	t = 0;
		for (auto &i : *this) {
			i.a = *p++;
			i.b = t;
			t += 1 / 30.f;
		}
		Optimise(*this, eps);
	}
	template<typename P> ISO_openarray<P> MakeValueArray(const ISO::Type *type) {
		ISO_openarray<P> dest;
		P*		p = dest.Create((uint32)B::size());
		for (auto &i : *this)
			*p++ = i.a;
		return dest;
	}
	Keys MakeKeyArray() {
		Keys dest((uint32)B::size());
		uint16	*p = dest;
		for (auto &i : *this)
			*p++ = int(i.b * 30.f + 0.5f);
		return dest;
	}
};

ISO_ptr<Animation> CompressedAnimation(ISO_ptr<Animation> anim) {
	ISO_ptr<Animation>	comp(anim.ID());

	for (int i = 0, nsubs = anim->Count(); i < nsubs; i++) {
		ISO_ptr<void>	in = (*anim)[i];

		if (in.GetType() == ISO::getdef<Animation>()) {
			if (ISO_ptr<Animation> out = CompressedAnimation((ISO_ptr<Animation>)in)) {
				if (nsubs == 1)
					return out;
					
				comp->Append(out);
			}

		} else if (in.GetType()->SameAs<ISO_openarray<ISO::Array<float,0> > >()) {
			ISO::Browser		array(in);
			size_t			count	= array.Count();
			ISO::Browser		first	= array[0];
			const ISO::Type	*type	= first.GetTypeDef();

			if (count == 1) {
				ISO_ptr<void>	out = MakePtr(type, in.ID());
				memcpy(out, first, type->GetSize());
				comp->Append(out);

			} else {
				tag2	id		= in.ID();
				auto	*type2	= new(2) ISO::TypeComposite;

				type2->Add<ISO_openarray<uint16> >("keys");
				type2->Add(array.GetTypeDef(), "values");

				ISO_ptr<void>	out	= MakePtr(type2, id);


				switch (first.Count()) {
					case 2:	{
						CompressedStream<float2>	cs((float2p*)first, count, 0.001f);
						KeyedStream<float2p>	*p	= out;
						p->keys		= cs.MakeKeyArray();
						p->values	= cs.MakeValueArray<float2p>(type);
						break;
					}
					case 3: {
						CompressedStream<float3>	cs((float3p*)first, count, 0.001f);
						KeyedStream<float3p>	*p	= out;
						p->keys		= cs.MakeKeyArray();
						p->values	= cs.MakeValueArray<float3p>(type);
						break;
					}
					case 4: {
						CompressedStream<float4>	cs((packed<float4>*)first, count, 0.001f);
						if (id == "rot") {
							type2->count--;
							type2->Add<ISO_openarray<compressed_quaternion> >("values");
							out			= MakePtr(type2, "comp_rot");
							KeyedStream<compressed_quaternion>	*p	= out;
							p->keys		= cs.MakeKeyArray();
							p->values	= cs.MakeValueArray<compressed_quaternion>(type);
						} else {
							KeyedStream<float4p>	*p	= out;
							p->keys		= cs.MakeKeyArray();
							p->values	= cs.MakeValueArray<float4p>(type);
						}
						break;
					}
				}
				comp->Append(out);
			}

		} else if (in.IsID("rot")) {
			ISO_openarray<float4p>&	a_in	= *(ISO_openarray<float4p>*)in;
			int						nf_in	= a_in.Count();
			ISO_ptr<ISO_openarray<compressed_quaternion> >	out("comp_rot", nf_in);
			bool					blank	= true;
			for (int i = 0; i < nf_in; i++) {
				quaternion	result = quaternion(float4(a_in[i]));
				if (blank && abs(result.v.w - one) > 0.001f)
					blank = false;
				(*out)[i] = result;
			}
			if (!blank)
				comp->Append(out);

		} else {
			comp->Append(in);
		}
	}

	if (*comp)
		return comp;
	return ISO_NULL;
}

ISO_ptr<Animation> AdditiveAnimation(ISO_ptr<Animation> anim, ISO_ptr<Animation> base, int frame0) {
	ISO_ptr<Animation>	add(anim.ID());
	int	nsubs = anim->Count();

	for (int i = 0; i < nsubs; i++) {
		ISO_ptr<void>	in = (*anim)[i], sub = (*base)[in.ID()];

		if (!sub) {
			add->Append(in);
			continue;
		}

		if (in.GetType() == ISO::getdef<Animation>()) {
			if (ISO_ptr<Animation> out = AdditiveAnimation(in, sub, frame0))
				add->Append(out);
			continue;
		}

		int	nf_in	= ((ISO_openarray<char>*)in)->Count();
		int	nf_sub	= ((ISO_openarray<char>*)sub)->Count();

		if (in.IsID("rot")) {
			auto	a_in	= ((ISO_openarray<float4p>*)in)->View();
			auto	a_sub	= ((ISO_openarray<float4p>*)sub)->View();
//			ISO_ptr<ISO_openarray<compressed_quaternion> >	out("comp_rot", nf_in);
			ISO_ptr<ISO_openarray<float4p> >	out("rot", nf_in);
			bool		blank	= true;
			quaternion	offset	= nf_sub && frame0 ? ~quaternion(float4(a_sub[0])) : quaternion(identity);
			for (int i = 0; i < nf_in; i++) {
				if (nf_sub && !frame0)
					offset = ~quaternion(float4(a_sub[i % nf_sub]));
				quaternion	result = quaternion(float4(a_in[i])) * offset;
				if (blank && abs(result.v.w - one) > 0.001f)
					blank = false;
				(*out)[i] = result.v;
			}
			if (!blank)
				add->Append(out);

		} else if (in.IsID("pos")) {
			auto	a_in	= ((ISO_openarray<float3p>*)in)->View();
			auto	a_sub	= ((ISO_openarray<float3p>*)sub)->View();
			ISO_ptr<ISO_openarray<float3p> >	out("pos", nf_in);
			bool	blank	= true;
			float3	offset	= nf_sub && frame0 ? float3(a_sub[0]) : float3(zero);
			for (int i = 0; i < nf_in; i++) {
				if (nf_sub && !frame0)
					offset = a_sub[i % nf_sub];
				float3	result = float3(a_in[i]) - offset;
				if (blank && len2(result) > 0.001f)
					blank = false;
				(*out)[i] = result;
			}
			if (!blank)
				add->Append(out);

		} else if (in.IsID("scale")) {
			auto	a_in	= ((ISO_openarray<float3p>*)in)->View();
			auto	a_sub	= ((ISO_openarray<float3p>*)sub)->View();
			ISO_ptr<ISO_openarray<float3p> >	out("scale", nf_in);
			bool	blank	= true;
			float3	offset	= nf_sub && frame0 ? float3{1 / a_sub[0].x, 1 / a_sub[0].y, 1 / a_sub[0].z} : float3(one);
			for (int i = 0; i < nf_in; i++) {
				if (nf_sub && !frame0) {
					float3p	&t = a_sub[i % nf_sub];
					offset = float3{1 / t.x, 1 / t.y, 1 / t.z};
				}
				float3	result = float3(a_in[i]) * offset;
				if (blank && len2(result - float3(one)) > 0.01f)
					blank = false;
				(*out)[i] = result;
			}
			if (!blank)
				add->Append(out);

		} else {
			add->Append(in);
		}
	}
	if (*add)
		return add;
	return ISO_NULL;
}

ISO_ptr<Animation> CompAdditiveAnimation(ISO_ptr<Animation> anim, ISO_ptr<Animation> base, int frame0) {
	return CompressedAnimation(AdditiveAnimation(anim, base, frame0));
}

ISO_ptr<Animation> ConcatAnimations(ISO_ptr<Animation> anim1, ISO_ptr<Animation> anim2) {
	ISO_ptr<Animation>	anim(anim1.ID());// = Duplicate(anim1, anim1.ID());
	uint32	len1 = 0, len2 = 0;
	for (int i = 0, nsubs = anim1->Count(); i < nsubs; i++) {
		ISO_ptr<void>	in1 = (*anim1)[i];
		ISO_ptr<void>	in2 = (*anim2)[in1.ID()];

		if (!in2) {
			anim->Append(in1);

		} else if (in1.GetType() == ISO::getdef<Animation>()) {
			anim->Append(ConcatAnimations(in1, in2));

		} else {
			ISO_ptr<void>	out = Duplicate(in1);
			ISO::Browser	b1(out);
			ISO::Browser	b2(in2);
			len1	= b1.Count();
			len2	= b2.Count();
			b1.Resize(len1 + len2);
			memcpy(b1[len1], b2[0], b2[0].GetSize() * len2);
			anim->Append(out);
		}
	}

	for (int i = 0, nsubs = anim2->Count(); i < nsubs; i++) {
		ISO_ptr<void>	in2 = (*anim2)[i];
		ISO_ptr<void>	in1 = (*anim1)[in2.ID()];

		if (in1)
			continue;
		if (in2.GetType() == ISO::getdef<Animation>()) {
		} else {
			ISO_ptr<void>	out = Duplicate(in2);
			ISO::Browser	b1(out);
			ISO::Browser	b2(in2);
			len2	= b2.Count();
			b1.Resize(len1 + len2);
			memset(b1[0], 0, b2[0].GetSize() * len1);
			memcpy(b1[len1], b2[0], b2[0].GetSize() * len2);
			anim->Append(out);
		}
	}

	return anim;
}
//-----------------------------------------------------------------------------
//	init
//-----------------------------------------------------------------------------

static initialise init(
	ISO_get_operation(CompressedAnimation),
	ISO_get_operation(AdditiveAnimation),
	ISO_get_operation(CompAdditiveAnimation)
);
