#include "render_output.h"

namespace iso {

struct DefaultRenderOutput : RenderOutput, Graphics::Display {
	DefaultRenderOutput(RenderWindow *window, const point &size)	{ SetSize(window, size); }
	void	SetSize(RenderWindow *window, const point &size)		{ Graphics::Display::SetSize(window, size); }
	Flags	GetFlags()									{ return NONE; }
	point	DisplaySize()								{ return Graphics::Display::Size(); }
	void	BeginFrame(GraphicsContext &ctx)			{ graphics.BeginScene(ctx);	}
	void	EndFrame(GraphicsContext &ctx)				{
		MakePresentable(ctx);
		graphics.EndScene(ctx);
		Present();
	}
	RenderView	GetView(int i) {
		point	size = Size();
		return {
			identity,
			float2{one, float(size.y) / size.x}.xyxy,
			rect(zero, Size()),
			Graphics::Display::GetDispSurface()
		};
	}
};

RenderOutput::Flags	RenderOutputFinder::AllCapabilities(RenderOutput::Flags flags) {
	RenderOutput::Flags	caps = RenderOutput::NONE;
	for (auto i = RenderOutputFinder::begin(); i; ++i)
		caps = caps | i->Capability(flags);
	return caps;
}
RenderOutput *RenderOutputFinder::FindAndCreate(RenderWindow *window, const point &size, RenderOutput::Flags flags) {
	for (auto i = RenderOutputFinder::begin(); i; ++i) {
		if (RenderOutput *o = i->Create(window, size, flags))
			return o;
	}
	return new DefaultRenderOutput(window, size);
}

} // namespace iso