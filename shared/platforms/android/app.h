#ifndef APP_H
#define APP_H

#include "render.h"
#include "filename.h"
#include <android/sensor.h>
#include <android/looper.h>

namespace iso {
	extern const char	*user_dir, *docs_dir;
	
	//-----------------------------------------------------------------------------
	//	RenderOutput
	//-----------------------------------------------------------------------------
	class RenderOutput {
	public:
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
		};
		friend inline Flags operator|(Flags a, Flags b) {
			return Flags(int(a) | int(b));
		}
	protected:
		EGLDisplay		display;
		EGLSurface		surface;
		EGLContext		context;

		Point			size;
		Flags			flags;

		void			Clear();
	public:
		RenderOutput() : size(0, 0), flags(NONE) {}
		~RenderOutput() { Clear(); }
		void			SetDisplay(ANativeWindow *window);

		Flags			GetFlags()			const	{ return flags; }
		Point			RenderSize()		const	{ return size; }
		Point			DisplaySize()		const	{ return size; }
		void			BeginFrame(GraphicsContext &ctx);
		void			EndFrame(GraphicsContext &ctx);
		RenderView		GetView(int i);
	};
	
	//-----------------------------------------------------------------------------
	//	Application
	//-----------------------------------------------------------------------------

	struct poll_source {
		enum ID {
			ID_MAIN		= 1,	// ID of commands coming from the app's main thread, which is returned as an identifier from ALooper_pollOnce(). The data for this identifier is a pointer to an poll_source structure. These can be retrieved and processed with read_cmd() and exec_cmd().
			ID_INPUT	= 2,	// ID of events coming from the AInputQueue of the application's window, which is returned as an identifier from ALooper_pollOnce().  The data for this identifier is a pointer to an poll_source structure.  These can be read via the inputQueue object of android_app.
			ID_USER		= 3,	// Start of user-defined ALooper identifiers.
		};
		int			id;
		void		*me;
		void		(*f)(void *p, poll_source* source);

		poll_source()  {}
		poll_source(ID _id, void *_me, void (*_f)(void*, poll_source*)) : id(_id), me(_me), f(_f)  {}
		void	set(ID _id, void *_me, void(*_f)(void*, poll_source*)) {
			id		= _id;
			me		= _me;
			f		= _f;
		}
		void	process() {
			(*f)(me, this);
		}
	};

	struct saved_state {
		float	angle;
		int		x;
		int		y;
	};

	class Application {
		ASensorManager*		sensorManager;
		ASensorEventQueue*	sensorEventQueue;
		const ASensor*		accelerometerSensor;
		bool				focus;
		GraphicsContext		ctx;
		RenderOutput		*output;

		void				process_input();
		static void			_process_input(void *me, poll_source* source)	{ ((Application*)me)->process_input(); }
	public:
		saved_state			state;
		int					init_display();
		void				draw_frame();
		void				term_display();

		void				SetDisplay(ANativeWindow *window)		{ output->SetDisplay(window); }
		void				SetFocus(bool _focus);

		GraphicsContext&	Context()								{ return ctx;	}
		RenderOutput::Flags	Capability(RenderOutput::Flags flags) const;
		bool				SetOutput(const Point &size, RenderOutput::Flags flags);
		RenderOutput::Flags	GetOutputFlags()	const				{ return output->GetFlags(); }
		Point				DisplaySize()		const				{ return output->DisplaySize(); }
		RenderView			GetView(int i)		const				{ return output->GetView(i); }

		Application(const char *title);
		int			Run();
		virtual	bool Update()=0;
		virtual	void Render()=0;
	};

	static inline filename	UserDir()		{ return user_dir;	}
	static inline filename	DocsDir()		{ return docs_dir;	}
	static inline bool		Restarted()		{ return false;		}
	static inline bool		Patched()		{ return false; }
}

int IsoMain(const char *cmdline = NULL);

#endif