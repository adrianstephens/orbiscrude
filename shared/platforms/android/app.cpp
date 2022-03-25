#include "app.h"
#include "utilities.h"
#include "jni_helper.h"

#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <sys/resource.h>

#include <EGL/egl.h>
#include <GLES/gl.h>

#include <poll.h>
#include <pthread.h>
#include <sched.h>

#include <android/configuration.h>
#include <android/native_activity.h>

#include <asm/sigcontext.h>
#include <unwind.h>
#include <dlfcn.h>

using namespace iso;

struct sigaction	old_sa[NSIG];

struct unwind_state {
	enum {N = 32};
	int			skip;
	int			size;
	uintptr_t	frames[N];

	unwind_state(int _skip) : skip(_skip), size(0) {}

	_Unwind_Reason_Code add(struct _Unwind_Context* context) {
		if (const uintptr_t ip = _Unwind_GetIP(context)) {
			if (skip)
				--skip;
			else
				frames[size++] = ip;
		}
		return size == N ? _URC_END_OF_STACK : _URC_NO_REASON;
	}
};

static _Unwind_Reason_Code unwind_callback(struct _Unwind_Context* context, void* arg) {
	return ((unwind_state*)arg)->add(context);
}

#if 0
using unw_cursor_t = spacer<32>;
using unw_context_t = spacer<32>;
extern "C" {
extern int unw_getcontext(unw_context_t *);
extern int unw_init_local(unw_cursor_t *, unw_context_t *);
extern int unw_step(unw_cursor_t *);
}

_Unwind_Reason_Code _Unwind_Backtrace2(_Unwind_Trace_Fn callback, void *ref, ucontext *uc) {
	unw_cursor_t cursor;
	unw_init_local(&cursor, (unw_context_t*)uc);

#if defined(_LIBUNWIND_ARM_EHABI)
	// Create a mock exception object for force unwinding.
	_Unwind_Exception ex;
	memset(&ex, '\0', sizeof(ex));
	ex.exception_class = 0x434C4E47554E5700; // CLNGUNW\0
#endif

  // walk each frame
	while (true) {
		_Unwind_Reason_Code result;

#if !defined(_LIBUNWIND_ARM_EHABI)
		// ask libunwind to get next frame (skip over first frame which is
		// _Unwind_Backtrace())
		if (unw_step(&cursor) <= 0)
			return _URC_END_OF_STACK;
#else
		// Get the information for this frame.
		unw_proc_info_t frameInfo;
		if (unw_get_proc_info(&cursor, &frameInfo) != UNW_ESUCCESS) {
			return _URC_END_OF_STACK;
		}

		// Update the pr_cache in the mock exception object.
		const uint32_t* unwindInfo = (uint32_t *)frameInfo.unwind_info;
		ex.pr_cache.fnstart = frameInfo.start_ip;
		ex.pr_cache.ehtp = (_Unwind_EHT_Header *)unwindInfo;
		ex.pr_cache.additional = frameInfo.flags;

		struct _Unwind_Context *context = (struct _Unwind_Context *)&cursor;
		// Get and call the personality function to unwind the frame.
		__personality_routine handler = (__personality_routine)frameInfo.handler;
		if (handler == NULL) {
			return _URC_END_OF_STACK;
		}
		if (handler(_US_VIRTUAL_UNWIND_FRAME | _US_FORCE_UNWIND, &ex, context) !=
			_URC_CONTINUE_UNWIND) {
			return _URC_END_OF_STACK;
		}
#endif // defined(_LIBUNWIND_ARM_EHABI)

		// call trace function with this frame
		result = (*callback)((struct _Unwind_Context *)(&cursor), ref);
		if (result != _URC_NO_REASON)
			return result;
	}
}
#endif

void signal_handler(int code, siginfo_t *si, void *p) {
	LOGE("SIGNAL(%i)", code);
	signal(code, SIG_DFL);	// Ensure we do not deadlock. Default of ALRM is to die. (signal() and alarm() are signal-safe)

/*	unwind_state	state(0);
	_Unwind_Backtrace2(unwind_callback, &state, (ucontext*)p);
	LOGE("Got %i frames", state.size);
	for (int i = 0; i < state.size; i++)
		LOGE("0x%08x", state.frames[i]);
*/
	old_sa[code].sa_handler(code);
}

int install_signal_handler(int sig) {
	struct sigaction sa;
	clear(sa);
	sa.sa_sigaction = signal_handler;
	sa.sa_flags		= SA_RESETHAND;
	int	res = sigaction(sig, &sa, &old_sa[sig]);
	return res ?  errno : 0;
}

void install_signal_handlers() {
	install_signal_handler(SIGILL);
	install_signal_handler(SIGABRT);
	install_signal_handler(SIGBUS);
	install_signal_handler(SIGFPE);
	install_signal_handler(SIGSEGV);
	install_signal_handler(SIGSTKFLT);
	install_signal_handler(SIGPIPE);
}

void launch_gdbserver(const char *path, int socket, int port = 5039) {
	bool	ready 	= false;
	int		pid 	= getpid();

	if (fork() == 0) {
//		dup2(socket, STDOUT_FILENO);
//		dup2(socket, STDERR_FILENO);

		fixed_string<32>	port_str, pid_str;
		port_str	<< "*:" << port;
		port_str	<< ':' << port;
		pid_str		<< pid;
		for (;;) {
			//execl(path, "gdbserver", port_str, "--attach", pid_str, (char*)NULL);
			execl(path, "lldb-server", "platform", "--listen", port_str, (char*)NULL);

			//if execl() succeeds, control will never reach here
			LOGE("Failed to launch %s %s --attach %s: error %d", path, (const char*)port_str, (const char*)pid_str, errno);
			while (!ready)
				sleep(2);
		}
	};
	while (!ready)
		sleep(2);
}

namespace iso {
	static Application		*application;
	const char	*user_dir;
	const char	*docs_dir;

	bool OpenGLThreadInit() {
		return true;
	}
	void OpenGLThreadDeInit() {
	}
}


void print_config(AConfiguration *config) {
	char lang[2], country[2];
	AConfiguration_getLanguage(config, lang);
	AConfiguration_getCountry(config, country);

	LOGV("Config: mcc=%d mnc=%d lang=%c%c cnt=%c%c orien=%d touch=%d dens=%d "
		"keys=%d nav=%d keysHid=%d navHid=%d sdk=%d size=%d long=%d "
		"modetype=%d modenight=%d",
		AConfiguration_getMcc(config),
		AConfiguration_getMnc(config),
		lang[0], lang[1], country[0], country[1],
		AConfiguration_getOrientation(config),
		AConfiguration_getTouchscreen(config),
		AConfiguration_getDensity(config),
		AConfiguration_getKeyboard(config),
		AConfiguration_getNavigation(config),
		AConfiguration_getKeysHidden(config),
		AConfiguration_getNavHidden(config),
		AConfiguration_getSdkVersion(config),
		AConfiguration_getScreenSize(config),
		AConfiguration_getScreenLong(config),
		AConfiguration_getUiModeType(config),
		AConfiguration_getUiModeNight(config)
	);
}


struct android_app {
	enum CMD {
		INPUT_CHANGED,			// the AInputQueue has changed.  Upon processing this command, android_app->inputQueue will be updated to the new queue (or NULL).
		INIT_WINDOW,			// a new ANativeWindow is ready for use.  Upon receiving this command, android_app->window will contain the new window surface.
		TERM_WINDOW,			// the existing ANativeWindow needs to be terminated.  Upon receiving this command, android_app->window still contains the existing window; after calling exec_cmd it will be set to NULL.
		WINDOW_RESIZED,			// the current ANativeWindow has been resized. Please redraw with its new size.
		WINDOW_REDRAW_NEEDED,	// the system needs that the current ANativeWindow be redrawn.  You should redraw the window before handing this to exec_cmd() in order to avoid transient drawing glitches.
		CONTENT_RECT_CHANGED,	// the content area of the window has changed, such as from the soft input window being shown or hidden.  You can find the new content rect in contentRect.
		GAINED_FOCUS,			// the app's activity window has gained input focus.
		LOST_FOCUS,				// the app's activity window has lost input focus.
		CONFIG_CHANGED,			// the current device configuration has changed.
		LOW_MEMORY,				// the system is running low on memory. Try to reduce your memory use.
		START,					// the app's activity has been started.
		RESUME,					// the app's activity has been resumed.
		SAVE_STATE,				// the app should generate a new saved state for itself, to restore from later if needed.  If you have saved state, allocate it with malloc and place it in android_app.savedState with the size in android_app.savedStateSize.  The will be freed for you later.
		PAUSE,					// the app's activity has been paused.
		STOP,					// the app's activity has been stopped.
		DESTROY,				// the app's activity is being destroyed, and waiting for the app thread to clean up and exit before proceeding.

		INVALID	= 1,
	};

	AConfiguration*		config;				// The current configuration the app is running in.
	ALooper*			looper;				// The ALooper associated with the app's thread.
	AInputQueue*		inputQueue;			// When non-NULL, this is the input queue from which the app will receive user input events.
	ANativeWindow*		window;				// When non-NULL, this is the window surface that the app can draw in.
	ARect				contentRect;		// Current content rectangle of the window; this is the area where the window's content should be placed to be seen by the user.
	CMD					activityState;		// Current state of the app's activity.  May be either START, RESUME, PAUSE, or STOP; see below.
	bool				destroyRequested;	// This is true when the application's NativeActivity is being destroyed and waiting for the app thread to complete.

	malloc_block		savedState;

	// -------------------------------------------------
	// Below are "private" implementation of the glue code.

	pthread_mutex_t		mutex;
	pthread_cond_t		cond;
	pthread_t			thread;

	int					msgread;
	int					msgwrite;

	poll_source			cmdPollSource;
	poll_source			inputPollSource;

	bool				running;
	bool				stateSaved;
	bool				destroyed;
	bool				redrawNeeded;
	AInputQueue*		pendingInputQueue;
	ANativeWindow*		pendingWindow;
	ARect				pendingContentRect;

	void			lock()		{ pthread_mutex_lock(&mutex); }
	void			wait()		{ pthread_cond_wait(&cond, &mutex); }
	void			unlock()	{ pthread_mutex_unlock(&mutex); }

	void	start(ANativeActivity* activity) {
		config = AConfiguration_new();
		AConfiguration_fromAssetManager(config, activity->assetManager);

		print_config(config);

		cmdPollSource.set(poll_source::ID_MAIN, this, _process_cmd);

		looper = ALooper_prepare(ALOOPER_PREPARE_ALLOW_NON_CALLBACKS);
		ALooper_addFd(looper, msgread, poll_source::ID_MAIN, ALOOPER_EVENT_INPUT, NULL, &cmdPollSource);

		lock();
		running = true;
		pthread_cond_broadcast(&cond);
		unlock();

		if (savedState)
			application->state = *(saved_state*)savedState;
	}

	void	shutdown(ANativeActivity* activity) {
		LOGV("destroy!");
		free_saved_state();
		lock();
		if (inputQueue)
			AInputQueue_detachLooper(inputQueue);
		AConfiguration_delete(config);
		destroyed = true;
		pthread_cond_broadcast(&cond);
		unlock();
	}

	void	process_cmd(ANativeActivity* activity, CMD cmd);

	static void		_process_cmd(void *p, poll_source* source) {
		ANativeActivity	*activity	= (ANativeActivity*)p;
		android_app		*app		= (android_app*)activity->instance;
		app->process_cmd(activity, app->read_cmd());
	}

	static void		*thread_entry(void *p) {
		ANativeActivity	*activity	= (ANativeActivity*)p;
		android_app		*app		= (android_app*)activity->instance;

		app->start(activity);
		IsoMain();
		app->shutdown(activity);

		return 0;
	}

	CMD				read_cmd();
	void			write_cmd(CMD cmd);
	void			set_input(AInputQueue* inputQueue);
	void			set_window(ANativeWindow* window);
	void			set_activity_state(CMD cmd);
	void*			save_state(size_t* outLen);

	void			free_saved_state();

	// -------------------------------------------------

	android_app(ANativeActivity* activity, const memory_block &_savedState);
	~android_app();
	void	spawn_thread(ANativeActivity* activity);
};

android_app	*the_app;

android_app::android_app(ANativeActivity* activity, const memory_block &_savedState) : savedState(_savedState) {
	the_app				= this;

	::memset(this, 0, sizeof(android_app));

	pthread_mutex_init(&mutex, NULL);
	pthread_cond_init(&cond, NULL);

	int msgpipe[2];
	if (pipe(msgpipe)) {
		LOGE("could not create pipe: %s", strerror(errno));
		return;
	}
	msgread		= msgpipe[0];
	msgwrite	= msgpipe[1];
}

void android_app::spawn_thread(ANativeActivity* activity) {
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thread, &attr, thread_entry, activity);

	// Wait for thread to start.
	lock();
	while (!running)
		wait();
	unlock();
}

android_app::~android_app() {
	lock();
	write_cmd(DESTROY);
	while (!destroyed)
		wait();
	unlock();

	close(msgread);
	close(msgwrite);
	pthread_cond_destroy(&cond);
	pthread_mutex_destroy(&mutex);
}


void android_app::free_saved_state() {
	lock();
	savedState.clear();
	unlock();
}

void android_app::process_cmd(ANativeActivity* activity, CMD cmd) {
	switch (cmd) {
		case INPUT_CHANGED:
			LOGV("INPUT_CHANGED\n");
			lock();
			if (inputQueue)
				AInputQueue_detachLooper(inputQueue);
			inputQueue = pendingInputQueue;
			if (inputQueue) {
				LOGV("Attaching input queue to looper");
				AInputQueue_attachLooper(inputQueue, looper, poll_source::ID_INPUT, NULL, &inputPollSource);
			}
			pthread_cond_broadcast(&cond);
			unlock();
			break;

		case INIT_WINDOW:
			LOGV("INIT_WINDOW\n");
			lock();
			window = pendingWindow;
			pthread_cond_broadcast(&cond);
			unlock();

			// The window is being shown, get it ready.
			if (window)
				application->SetDisplay(window);
			break;

		case TERM_WINDOW:
			LOGV("TERM_WINDOW\n");
			pthread_cond_broadcast(&cond);
			// The window is being hidden or closed, clean it up.
			application->SetDisplay(0);

			lock();
			window = NULL;
			pthread_cond_broadcast(&cond);
			unlock();
			break;

		case SAVE_STATE:
			LOGV("SAVE_STATE\n");
			// The system has asked us to save our current state.  Do so.
			savedState = memory_block(&application->state);
			lock();
			stateSaved = true;
			pthread_cond_broadcast(&cond);
			unlock();
			break;

		case GAINED_FOCUS:
			application->SetFocus(true);
			break;

		case LOST_FOCUS:
			application->SetFocus(false);
			break;

		case RESUME:
		case START:
		case PAUSE:
		case STOP:
			LOGV("activityState=%d\n", cmd);
			lock();
			activityState = cmd;
			pthread_cond_broadcast(&cond);
			unlock();

			if (cmd == RESUME)
				free_saved_state();
			break;

		case CONFIG_CHANGED:
			LOGV("CONFIG_CHANGED\n");
			AConfiguration_fromAssetManager(config, activity->assetManager);
			print_config(config);
			break;

		case DESTROY:
			LOGV("DESTROY\n");
			destroyRequested = true;
			break;

		default:
			break;
	}
}

//-----------------------------------------------------------------------------

android_app::CMD android_app::read_cmd() {
	int8_t cmd;
	if (read(msgread, &cmd, sizeof(cmd)) == sizeof(cmd)) {
		switch (cmd) {
			case SAVE_STATE:
				free_saved_state();
				break;
		}
		return (CMD)cmd;
	} else {
		LOGE("No data on command pipe!");
	}
	return INVALID;
}

void android_app::write_cmd(CMD cmd) {
	int8	c = cmd;
	if (write(msgwrite, &c, sizeof(c)) != sizeof(c))
		LOGE("Failure writing app cmd: %s\n", strerror(errno));
}

void android_app::set_input(AInputQueue* inputQueue) {
	lock();
	pendingInputQueue = inputQueue;
	write_cmd(INPUT_CHANGED);
	while (inputQueue != pendingInputQueue)
		wait();
	unlock();
}

void android_app::set_window(ANativeWindow* window) {
	lock();
	if (pendingWindow)
		write_cmd(TERM_WINDOW);
	pendingWindow = window;
	if (window)
		write_cmd(INIT_WINDOW);
	while (window != pendingWindow)
		wait();
	unlock();
}

void android_app::set_activity_state(CMD cmd) {
	lock();
	write_cmd(cmd);
	while (activityState != cmd)
		wait();
	unlock();
}

void* android_app::save_state(size_t* outLen) {
	lock();
	stateSaved = false;
	write_cmd(android_app::SAVE_STATE);
	while (!stateSaved)
		wait();

	void* ret = savedState;
	if (ret) {
		*outLen		= savedState.length();
		savedState.clear();
	}

	unlock();
	return ret;
}

//-----------------------------------------------------------------------------

extern "C" {
void ANativeActivity_onCreate(ANativeActivity* activity, void* savedState, size_t savedStateSize) {
//	install_signal_handlers();
	LOGV("Creating: %p\n", activity);

	jni::ObjectHolder::env = activity->env;
	jni::Object	jactivity(activity->clazz);
	
	auto	path = jactivity.Call<java::lang::String>("getPackageCodePath");
//	auto	path = jactivity.Call<java::lang::String>("getPackageCodePath");
//	auto	path = jactivity.Call<string>("getPackageCodePath");
	LOGE("package path=%s", (const char*)path.chars_utf8());
	launch_gdbserver("/data/data/com.isoview/lib/gdbserver", 0);
//	launch_gdbserver(filename(path).dir().add_dir("lib/armeabi-v7a/gdbserver"), 0);

	activity->callbacks->onDestroy = [](ANativeActivity* activity) {
		LOGV("Destroy: %p\n", activity);
		delete (android_app*)activity->instance;
	};
	activity->callbacks->onStart = [](ANativeActivity* activity) {
		LOGV("Start: %p\n", activity);
		((android_app*)activity->instance)->set_activity_state(android_app::START);
	};
	activity->callbacks->onResume = [](ANativeActivity* activity) {
		LOGV("Resume: %p\n", activity);
		((android_app*)activity->instance)->set_activity_state(android_app::RESUME);
	};
	activity->callbacks->onSaveInstanceState = [](ANativeActivity* activity, size_t* outLen) {
		LOGV("SaveInstanceState: %p\n", activity);
		return ((android_app*)activity->instance)->save_state(outLen);
	};
	activity->callbacks->onPause = [](ANativeActivity* activity) {
		LOGV("Pause: %p\n", activity);
		((android_app*)activity->instance)->set_activity_state(android_app::PAUSE);
	};
	activity->callbacks->onStop = [](ANativeActivity* activity) {
		LOGV("Stop: %p\n", activity);
		((android_app*)activity->instance)->set_activity_state(android_app::STOP);
	};
	activity->callbacks->onConfigurationChanged = [](ANativeActivity* activity) {
		LOGV("ConfigurationChanged: %p\n", activity);
		((android_app*)activity->instance)->write_cmd(android_app::CONFIG_CHANGED);
	};
	activity->callbacks->onLowMemory = [](ANativeActivity* activity) {
		LOGV("LowMemory: %p\n", activity);
		((android_app*)activity->instance)->write_cmd(android_app::LOW_MEMORY);
	};
	activity->callbacks->onWindowFocusChanged = [](ANativeActivity* activity, int focused) {
		LOGV("WindowFocusChanged: %p -- %d\n", activity, focused);
		((android_app*)activity->instance)->write_cmd(focused ? android_app::GAINED_FOCUS : android_app::LOST_FOCUS);
	};
	activity->callbacks->onNativeWindowCreated = [](ANativeActivity* activity, ANativeWindow* window) {
		LOGV("NativeWindowCreated: %p -- %p\n", activity, window);
		((android_app*)activity->instance)->set_window(window);
	};
	activity->callbacks->onNativeWindowDestroyed = [](ANativeActivity* activity, ANativeWindow* window) {
		LOGV("NativeWindowDestroyed: %p -- %p\n", activity, window);
		((android_app*)activity->instance)->set_window(NULL);
	};
	activity->callbacks->onInputQueueCreated = [](ANativeActivity* activity, AInputQueue* queue) {
		LOGV("InputQueueCreated: %p -- %p\n", activity, queue);
		((android_app*)activity->instance)->set_input(queue);
	};
	activity->callbacks->onInputQueueDestroyed = [](ANativeActivity* activity, AInputQueue* queue) {
		LOGV("InputQueueDestroyed: %p -- %p\n", activity, queue);
		((android_app*)activity->instance)->set_input(NULL);
	};
	android_app *app = new android_app(activity, memory_block(savedState, savedStateSize));
	activity->instance = app;
	app->spawn_thread(activity);
}
}

//-----------------------------------------------------------------------------
#if 0
static GLint vertices[][3] = {
	{ -0x10000, -0x10000, -0x10000 },
	{ 0x10000, -0x10000, -0x10000 },
	{ 0x10000,  0x10000, -0x10000 },
	{ -0x10000,  0x10000, -0x10000 },
	{ -0x10000, -0x10000,  0x10000 },
	{ 0x10000, -0x10000,  0x10000 },
	{ 0x10000,  0x10000,  0x10000 },
	{ -0x10000,  0x10000,  0x10000 }
};

static GLint colors[][4] = {
	{ 0x00000, 0x00000, 0x00000, 0x10000 },
	{ 0x10000, 0x00000, 0x00000, 0x10000 },
	{ 0x10000, 0x10000, 0x00000, 0x10000 },
	{ 0x00000, 0x10000, 0x00000, 0x10000 },
	{ 0x00000, 0x00000, 0x10000, 0x10000 },
	{ 0x10000, 0x00000, 0x10000, 0x10000 },
	{ 0x10000, 0x10000, 0x10000, 0x10000 },
	{ 0x00000, 0x10000, 0x10000, 0x10000 }
};

GLubyte indices[] = {
	0, 4, 5,    0, 5, 1,
	1, 5, 6,    1, 6, 2,
	2, 6, 7,    2, 7, 3,
	3, 7, 4,    3, 4, 0,
	4, 7, 6,    4, 6, 5,
	3, 0, 1,    3, 1, 2
};

void Cube_setupGL(double width, double height) {
	// Initialize GL state.
	glDisable(GL_DITHER);
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
	glClearColor(1.0f, 0.41f, 0.71f, 1.0f);
	glEnable(GL_CULL_FACE);
	glShadeModel(GL_SMOOTH);
	glEnable(GL_DEPTH_TEST);

	glViewport(0, 0, width, height);
	GLfloat ratio = (GLfloat)width / height;
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	glFrustumf(-ratio, ratio, -1, 1, 1, 10);
}

void Cube_tearDownGL() {
}

void Cube_prepare() {
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

void Cube_draw(float _rotation) {
	glMatrixMode(GL_MODELVIEW);
	glLoadIdentity();
	glTranslatef(0, 0, -3.0f);
	glRotatef(_rotation * 0.25f, 1, 0, 0);  // X
	glRotatef(_rotation, 0, 1, 0);          // Y

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);

	glFrontFace(GL_CW);
	glVertexPointer(3, GL_FIXED, 0, vertices);
	glColorPointer(4, GL_FIXED, 0, colors);
	glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_BYTE, indices);
}
#endif

//-----------------------------------------------------------------------------
//	Application
//-----------------------------------------------------------------------------

Application::Application(const char *title) {
	AppEvent(AppEvent::PRE_GRAPHICS).send();

	sensorManager		= ASensorManager_getInstanceForPackage("");
	accelerometerSensor = ASensorManager_getDefaultSensor(sensorManager, ASENSOR_TYPE_ACCELEROMETER);
	sensorEventQueue	= ASensorManager_createEventQueue(sensorManager, the_app->looper, poll_source::ID_USER, NULL, NULL);

	application		= this;
	graphics.Init();
}

void Application::SetFocus(bool _focus) {
	focus = _focus;
	if (focus) {
		// When our app gains focus, we start monitoring the accelerometer.
		if (accelerometerSensor) {
			ASensorEventQueue_enableSensor(sensorEventQueue, accelerometerSensor);
			// We'd like to get 60 events per second
			ASensorEventQueue_setEventRate(sensorEventQueue, accelerometerSensor, (1000L / 60) * 1000);
		}
	} else {
		// When our app loses focus, we stop monitoring the accelerometer.
		if (accelerometerSensor)
			ASensorEventQueue_disableSensor(sensorEventQueue, accelerometerSensor);
	}
}


void RenderOutput::Clear() {
	if (display != EGL_NO_DISPLAY) {
		eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
		if (context != EGL_NO_CONTEXT)
			eglDestroyContext(display, context);
		if (surface != EGL_NO_SURFACE)
			eglDestroySurface(display, surface);
		eglTerminate(display);
	}
	display		= EGL_NO_DISPLAY;
	context		= EGL_NO_CONTEXT;
	surface		= EGL_NO_SURFACE;
}

void RenderOutput::SetDisplay(ANativeWindow *window) {
	if (!window) {
		Clear();
		return;
	}

	// we select an EGLConfig with at least 8 bits per color component compatible with on-screen windows
	static const EGLint attribs[] = {
		EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
		EGL_BLUE_SIZE, 8,
		EGL_GREEN_SIZE, 8,
		EGL_RED_SIZE, 8,
		EGL_NONE
	};
	EGLint		format;
	EGLint		numConfigs;
	EGLConfig	config;

	display = eglGetDisplay(EGL_DEFAULT_DISPLAY);

	eglInitialize(display, 0, 0);

	eglChooseConfig(display, attribs, &config, 1, &numConfigs);
	eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &format);

	ANativeWindow_setBuffersGeometry(window, 0, 0, format);

	surface = eglCreateWindowSurface(display, config, window, NULL);
	context = eglCreateContext(display, config, NULL, NULL);

	if (eglMakeCurrent(display, surface, surface, context) == EGL_FALSE) {
		LOGW("Unable to eglMakeCurrent");
		return;
	}

	eglQuerySurface(display, surface, EGL_WIDTH, &size.x);
	eglQuerySurface(display, surface, EGL_HEIGHT, &size.y);

	//Cube_setupGL(width, height);

	// Initialize GL state.
//	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_FASTEST);
//	glEnable(GL_CULL_FACE);
//	glShadeModel(GL_SMOOTH);
//	glDisable(GL_DEPTH_TEST);
}

void RenderOutput::BeginFrame(GraphicsContext &ctx) {
	graphics.BeginScene(ctx);
}

void RenderOutput::EndFrame(GraphicsContext &ctx) {
	graphics.EndScene(ctx);
	eglSwapBuffers(display, surface);
}

RenderView RenderOutput::GetView(int i) {
	RenderView view = {
		identity,
		float2(one, float(size.y) / size.x).xyxy,
		Rect(Point(0,0), size),
		Surface(),
	};
	return view;
}

void Application::process_input() {
	AInputEvent* event = NULL;
	while (AInputQueue_getEvent(the_app->inputQueue, &event) >= 0) {
		LOGV("New input event: type=%d\n", AInputEvent_getType(event));
		if (AInputQueue_preDispatchEvent(the_app->inputQueue, event))
			continue;

		int handled = 0;
		if (AInputEvent_getType(event) == AINPUT_EVENT_TYPE_MOTION) {
			//state.x = AMotionEvent_getX(event, 0);
			//state.y = AMotionEvent_getY(event, 0);
			handled = 1;
		}

		AInputQueue_finishEvent(the_app->inputQueue, event, handled);
	}
}

RenderOutput::Flags	Application::Capability(RenderOutput::Flags flags) const {
	return RenderOutput::NONE;
}

bool Application::SetOutput(const Point &size, RenderOutput::Flags flags) {
	return true;
}


int Application::Run() {
	the_app->inputPollSource.set(poll_source::ID_INPUT, this, _process_input);

	int	id = 0;
	while (true) {//!exiting) {
		int				events;
		poll_source*	source;
		while ((id = ALooper_pollAll(0, NULL, &events, (void**)&source)) >= 0) {
			if (source)
				source->process();

			if (id == poll_source::ID_USER) {
				ASensorEvent event;
				while (ASensorEventQueue_getEvents(sensorEventQueue, &event, 1) > 0)
					LOGI("accelerometer: x=%f y=%f z=%f", event.acceleration.x, event.acceleration.y, event.acceleration.z);
			}
		}

		Update();
		output->BeginFrame(ctx);
		AppEvent(AppEvent::RENDER).send();
		output->EndFrame(ctx);
	}
	return 0;
}

extern "C" {
	JavaVM* jvm;

	JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM* vm, void* reserved) {
		jvm = vm;
		//install_signal_handlers();
		return JNI_VERSION_1_6;
	}

	JNIEXPORT void JNICALL JNI_OnUnload(JavaVM* vm, void* reserved) {
	}

}
