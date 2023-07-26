#ifndef APP_H
#define APP_H

#include "render_output.h"
#include "controller.h"
#include "base/tree.h"
#include "profiler.h"
#include "filename.h"

#ifndef PLAT_WINRT
#include "windows\window.h"
#endif

namespace iso {

	//-----------------------------------------------------------------------------
	//	Application
	//-----------------------------------------------------------------------------
	class Application : public aligner<16>
#ifndef PLAT_WINRT
		, public win::Window<Application>
#endif
	{
#ifdef PLAT_WINRT
		RenderWindow			*window;
#endif
		GraphicsContext			ctx;
		unique_ptr<RenderOutput>	output;
		bool					active;
		bool					exiting;
	public:

#ifndef PLAT_WINRT
		point					lmouse, rmouse;

		struct KeyboardKey : public e_treenode<KeyboardKey, false> {
			int					vk;
			ControllerButton	button;
			KeyboardKey(int _vk, ControllerButton _button) : vk(_vk), button(_button)	{}
			operator int()			const	{ return vk;		}
			bool	operator<(int k) const	{ return vk < k;	}
		};
		e_tree<KeyboardKey>	keys;

		static point		to_point(const win::Point &p)					{ return *(const point*)&p; }
		static float2		to_float2(const point &p)						{ return {float(p.x), float(p.y)}; }
		float2				NormalisedPos(const point &p)					{ return to_float2(p) / to_float2(to_point(GetClientRect().Size())) * 2 - 1; }
		void				AddKey(int vk, ControllerButton button)			{ keys.insert(new KeyboardKey(vk, button)); }
#endif

		void				BeginFrame()	{ output->BeginFrame(ctx); }
		void				EndFrame()		{ output->EndFrame(ctx); }

		GraphicsContext&	Context()										{ return ctx; }
		RenderOutput::Flags	Capability(RenderOutput::Flags flags)	const	{ return RenderOutputFinder::AllCapabilities(flags); }
		bool				SetOutput(const point &size, RenderOutput::Flags flags);

		RenderOutput::Flags	GetOutputFlags()						const	{ return output ? output->GetFlags() : RenderOutput::NONE; }
#ifdef PLAT_WINRT
		point				DisplaySize()							const	{ return output ? output->DisplaySize() : GetSize(window); }
#else
		point				DisplaySize()							const	{ return output ? output->DisplaySize() : GetSize((RenderWindow*)hWnd); }
#endif
		RenderView			GetView(int i)							const	{ return output->GetView(i); }
		auto				Views()									const	{ return output->Views(); }
		void				DrawHidden(GraphicsContext &ctx, const RenderView &v) const	{ return output->DrawHidden(ctx, v); }

		Application(const char *title);
		LRESULT		Proc(win::MSG_ID message, WPARAM wParam, LPARAM lParam);
		void		Run();
	};

	filename		UserDir();
	inline filename	DocsDir()		{ return UserDir();	}
}

int IsoMain(const char *cmdline);

#endif