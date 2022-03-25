#include "main.h"
#include "Graphics.h"
#include "Render.h"
#include "iso/iso_files.h"
#include "..\Systems\Particle\Particle.h"

using namespace app;

void InitParticleSystem();

class ViewParticle : public aligned<Window<ViewParticle>, 16>, public WindowTimer<ViewParticle>, Graphics::Display {
	World						world;
	ISO_ptr<particle_effect>	p;
	ParticleSet		partset;
	float			prevt;
	timer			time;

public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_SIZE: {
				Point	p(lParam);
				SetSize((RenderWindow*)hWnd, point{p.x, p.y});
				break;
			}
			case WM_PAINT: {
				DeviceContextPaint	dc(*this);
				point		size	= Size();

				GraphicsContext ctx;
				graphics.BeginScene(ctx);
				ctx.SetRenderTarget(GetDispSurface());
				ctx.SetZBuffer(Surface(TEXF_D24S8, size, MEM_DEPTH));
				ctx.Clear(colour(0.5f,0.5f,0.5f));

				ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);
				ctx.SetBlendEnable(true);

				ctx.SetBackFaceCull(BFC_BACK);
				ctx.SetDepthTest(DT_CLOSER);
				ctx.SetDepthWriteEnable(false);

				float		aspect	= float(size.x) / size.y;
				float3x4	view	= translate(0,0,4) * rotate_in_x(pi * half);
				float4x4	proj	= perspective_projection(1.f, aspect, .1f);

				ShaderConsts rs(view, proj, time);
				RenderEvent re(ctx, rs);
				re.SetShaderParams();
				partset.Render(&re, view, re.consts.proj);

				graphics.EndScene(ctx);
				Present();
				break;
			}

			case WM_ISO_TIMER: {
				float	t	= time;
				partset.Update(t - prevt);
				prevt		= t;
				Invalidate();
				break;
			}

			case WM_LBUTTONDOWN:
				SetFocus();
				break;

			case WM_NCDESTROY:
				delete this;
				break;
			default:
				return Super(message, wParam, lParam);
		}
		return 0;
	}

	ViewParticle(const WindowPos &wpos, const ISO_ptr<void> &_p) : p(_p), partset(0), prevt(0) {
		world.Use();
		world.Begin();
		InitParticleSystem();
		partset.Init(p.ID(), p, identity);
		Create(wpos, NULL, CHILD | VISIBLE, CLIENTEDGE);
		Timer::Start(1.f/60);
	}
};

class EditorParticle : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->Is("particle_effect");
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr<void> &p) {
		return *new ViewParticle(wpos,
			FileHandler::ExpandExternals(p)
		);
	}
} editorparticle;
