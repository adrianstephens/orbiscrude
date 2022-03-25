#ifndef RENDER_OUTPUT_H
#define RENDER_OUTPUT_H

#include "render.h"

namespace iso {

	struct RenderWindow;
	point GetSize(const RenderWindow *win);

	struct RenderOutput {
		enum Flags {
			NONE				= 0,

			ASPECT_4_3			= 0 << 0,
			ASPECT_16_9			= 1 << 0,

			DIMENSIONS_2		= 0 << 1,
			DIMENSIONS_3		= 1 << 1,

			_FORMAT_MASK		= 3 << 2,
			FORMAT_8BIT			= 0 << 2,
			FORMAT_10BIT		= 1 << 2,
			FORMAT_16BIT		= 2 << 2,
			FORMAT_FLOAT		= 3 << 2,

			_MSAA_MASK			= 3 << 4,
			MSAA_NONE			= 0 << 4,
			MSAA_2x				= 1 << 4,
			MSAA_4x				= 2 << 4,
		};

		friend Flags operator|(Flags a, Flags b) { return Flags(int(a) | int(b)); }
		virtual ~RenderOutput()	{}
		virtual void		SetSize(RenderWindow *window, const point &size)=0;
		virtual Flags		GetFlags()=0;
		virtual point		DisplaySize()=0;
		virtual void		BeginFrame(GraphicsContext &ctx)=0;
		virtual void		EndFrame(GraphicsContext &ctx)=0;
		virtual RenderView	GetView(int i)=0;
		virtual void		DrawHidden(GraphicsContext &ctx, const RenderView &v)	{}
		virtual virtual_container<RenderView>	Views() {
			return transformc(int_range(1), [this](int i) { return GetView(i); });
		}
	};

	class RenderOutputFinder : public static_list_priority<RenderOutputFinder> {
		RenderOutput::Flags	(*vCapability)(RenderOutputFinder *me, RenderOutput::Flags flags);// { return RenderOutput::NONE; }
		RenderOutput*		(*vCreate)(RenderOutputFinder *me, RenderWindow* window, const point& size, RenderOutput::Flags flags);
		RenderOutput::Flags	Capability(RenderOutput::Flags flags)										{ return vCapability(this, flags); }
		RenderOutput*		Create(RenderWindow *window, const point &size, RenderOutput::Flags flags)	{ return vCreate(this, window, size, flags); }

		//virtual RenderOutput::Flags	Capability(RenderOutput::Flags flags) { return RenderOutput::NONE; }
		//virtual RenderOutput*		Create(RenderWindow *window, const point &size, RenderOutput::Flags flags)=0;
	public:
		template<typename T> RenderOutputFinder(T*, int PRI) : static_list_priority<RenderOutputFinder>(PRI)
			, vCapability(make_staticfunc2(&T::Capability, RenderOutputFinder)), vCreate(make_staticfunc2(&T::Create, RenderOutputFinder)) {}
		static RenderOutput::Flags	AllCapabilities(RenderOutput::Flags flags);
		static RenderOutput*		FindAndCreate(RenderWindow *window, const point &size, RenderOutput::Flags flags);
	};

	template<typename T, int _PRI> class RenderOutputFinderPri : public RenderOutputFinder {
	public:
		RenderOutputFinderPri() : RenderOutputFinder((T*)nullptr, _PRI) {}
	};
	//template<typename T, int PRI> using RenderOutputFinderPri = static_list_priorityT<RenderOutputFinder, PRI>;

} // namespace iso
#endif //RENDER_OUTPUT_H