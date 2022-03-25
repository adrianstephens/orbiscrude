#include "tweak.h"
#include "graphics.h"
#include "shader.h"
#include "profiler.h"
#include "controller.h"
#include "font.h"
#include "utilities.h"
#include "postprocess/post.h"
#include "render.h"
#include "object.h"

using namespace iso;

class DebugPrinter : public FontPrinter {
	float4x4		matrix;
	float4			font_size;

public:
	DebugPrinter(GraphicsContext &_ctx, const Font *font);
	DebugPrinter&	Print(int x, int y, FontAlign align, const char *string, int len = -1);
	DebugPrinter&	PrintFramed(int x, int y, param(colour) backdrop, const char *string, int len = -1);
};

DebugPrinter::DebugPrinter(GraphicsContext &ctx, const Font *font) : FontPrinter(ctx, font) {
	matrix			= hardware_fix(parallel_projection_rect(0, 1280, 0, 720, -1, 1));
	font_size		= float4(reciprocal(font->Tex().Size()), float(font->outline), 0.5f);

	AddShaderParameter(ISO_CRC("worldViewProj", 0xb3f90db0),	matrix);
	AddShaderParameter(ISO_CRC("font_samp", 0x810bb451),		font->Tex());
	AddShaderParameter(ISO_CRC("font_size", 0xe7468594),		font_size);

	ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);
}

DebugPrinter &DebugPrinter::Print(int x, int y, FontAlign align, const char *string, int len) {
	Set(ctx, *root["data"]["menu"]["font_dist"][0]);
	At(x, y).SetAlign(align).FontPrinter::Print(string, len);
	return *this;
}

DebugPrinter& DebugPrinter::PrintFramed(int x, int y, param(colour) backdrop, const char *string, int len) {
	float tw, th;
	CalcRect(&tw, &th, string, len);

	AddShaderParameter(ISO_CRC("diffuse_colour", 0x6d407ef0), backdrop);
	Set(ctx, *root["data"]["default"]["coloured"][0]);
	PostEffects(ctx).DrawQuad(float2(x, y - GetFont()->baseline), float2(x + tw, y + th - GetFont()->baseline));

	Set(ctx, *root["data"]["menu"]["font_dist"][0]);
	At(x, y).FontPrinter::Print(string, len);
	return *this;
}

struct DebugFont : Handles2<DebugFont, AppEvent>, public ISO_ptr<Font> {
	void	operator()(AppEvent *ev) {
		if (ev->state == AppEvent::BEGIN)
			*(ISO_ptr<Font>*)this = root["data"]["debug_font"];
	}
} debug_font;

//-----------------------------------------------------------------------------

#if USE_TWEAKS

#define CBUT_TWEAKS			CBUT_SPECIAL
#define CBUT_TWEAKS_BACK	CBUT_B

TWEAK(int, TWEAKS_HSET, 180, 0, 255);
TWEAK(float, TWEAKS_SCROLL_SPEED, 3.0f);
TWEAK(float, TWEAKS_ADJUST_SPEED, 3.0f);

TWEAK(bool, STATS_ENABLE, false);
TWEAK(float, TWEAKS_STATS_LEFT_AMOUNT, 980.0f, 0.0f, 1280.0f);
TWEAK(float, TWEAKS_STATS_DOWN_AMOUNT, 34.0f, 0.0f, 720.0f);

#if 0
TWEAK(bool, FPS_ENABLE, false);
TWEAK(float, TWEAKS_FPS_LEFT_AMOUNT, 980.0f, 0.0f, 1280.0f);
TWEAK(float, TWEAKS_FPS_DOWN_AMOUNT, 600.0f, 0.0f, 720.0f);
#endif

TWEAK(float, TWEAKS_LEFT_AMOUNT, 73.6f, 0.0f, 100.0f);
#ifdef PLAT_WII
	TWEAK(float, TWEAKS_DOWN_AMOUNT, 82.0f, 0.0f, 100.0f);
#else
	TWEAK(float, TWEAKS_DOWN_AMOUNT, 34.1f, 0.0f, 100.0f);
#endif
TWEAK(float, TWEAKS_COMMENT_LEFT_AMOUNT, 107.0f, 0.0f, 1280.0f);
TWEAK(float, TWEAKS_COMMENT_DOWN_AMOUNT, 680.0f, 0.0f, 720.0f);

//-----------------------------------------------------------------------------

class TweakUI : public HandlesGlobal<TweakUI, FrameEvent2>, public HandlesGlobal<TweakUI, RenderEvent> {
	bool	enabled;
	int		focus, cursor;
	float	fcursor, down, hdown;

	tweak	*GetTweak(ISO_browser b, int i)	{
		dynamic_array<tweak*>	*t	= b;
		return (*t)[i];
	}
	void	DisplaySub(const ISO_browser &b, string_accum &buffer, int cursor);
public:
	void	Display(const Font *font);
	void	Update(Controller *cont, float dt);
	
	void	operator()(FrameEvent2 &ev) {
		Controller	*cont	= 0;
		float		dt		= ev.dt;
		Update(cont, dt);
	}

	void	operator()(RenderEvent &re) {
		if (enabled)
			Display(re.ctx, debug_font);
	}

	TweakUI() : enabled(false), focus(-1), cursor(0)	{}
} tweak_ui;

void TweakUI::DisplaySub(const ISO_browser &b, string_accum &buffer, int cursor) {
	for (int i = 0, n = b.Count(); i < n; i++) {
		// active
		buffer << cc_hset(TWEAKS_HSET);
		if (i == cursor)
			buffer << cc_colour(colour(1,1,0));
		buffer << cc_rightj << tag(b.GetName(i)) << cc_align_left << " : " << *GetTweak(b, i) << "\n";
		// restore color
		if (i == cursor)
			buffer << cc_restorecol;
	}
}

void TweakUI::Display(const Font *font) {
	DebugPrinter		dp(font);
	colour				backdrop(0,0,0,0.5f);
	buffer_accum<4096>	buffer;
	ISO_browser			tweaks = root["tweaks"];

	if (focus >= 0)
		buffer << cc_colour(colour(0.5f,0.5f,0.5f));
	for (int i = 0, n = tweaks.Count(); i < n; i++) {
		if (focus < 0 && i == cursor)
			buffer << cc_colour(colour(1,1,0)) << tweaks.GetName(i) << cc_restorecol;
		else
			buffer << tweaks.GetName(i);
		if (i == focus)
			buffer << "->";
		buffer << "\n";
	}

	if (focus >= 0) {
		buffer << cc_restorecol << cc_vset(0);
		DisplaySub(tweaks[focus], buffer, cursor);
	}

	dp.PrintFramed(TWEAKS_LEFT_AMOUNT, TWEAKS_DOWN_AMOUNT, backdrop, buffer);

	if (focus >= 0) {
		tweak		*t	= GetTweak(tweaks[focus], cursor);
		if (const char *comment = t->GetComment()) {
			dp.SetColour(colour(1,1,0));
			dp.PrintFramed(TWEAKS_COMMENT_LEFT_AMOUNT, TWEAKS_COMMENT_DOWN_AMOUNT, backdrop, comment);
		}
	}

	if (STATS_ENABLE) {
		DisplaySub(tweaks["Stats"], buffer.reset(), -1);
		dp.PrintFramed(TWEAKS_STATS_LEFT_AMOUNT, TWEAKS_STATS_DOWN_AMOUNT, backdrop, buffer);
	}

#if 0
	if (FPS_ENABLE) {
		DisplaySub(tweaks["Fps"], buffer.reset(), -1);
		dp.PrintFramed(TWEAKS_FPS_LEFT_AMOUNT, TWEAKS_FPS_DOWN_AMOUNT, backdrop, buffer);
	}
#endif
}

void TweakUI::Update(Controller *cont, float dt) {
	if (cont->MultiHit(CBUT_TWEAKS))
		enabled = !enabled;
	if (!enabled)
		return;

	ISO_browser	b	= root["tweaks"];
	int			n	= b.Count();
	if (n == 0)
		return;

	if (focus == -1) {
		if (cont->Hit(CBUT_ACTION)) {
			cont->Release(CBUT_ACTION);
			cont->EatDown(CBUT_ACTION);
			focus	= cursor;
			fcursor	=
			cursor	= 0;
		}
	} else {
		b	= b[focus];
		n	= b.Count();
	}

	if (cont->Hit(CBUT_TWEAKS_BACK)) {
		if (focus == -1) {
			enabled = false;
		} else {
			fcursor	=
			cursor	= focus;
			focus	= -1;
		}
		cont->Release(CBUT_TWEAKS_BACK);
		cont->EatDown(CBUT_TWEAKS_BACK);
	}

	if (cont->Down(CBUT_DPAD_UP | CBUT_DPAD_DOWN)) {
		float	adj;
		if (cont->Hit(CBUT_DPAD_UP | CBUT_DPAD_DOWN)) {
			cont->Release(CBUT_DPAD_UP | CBUT_DPAD_DOWN);
			down	= 0;
			adj		= 1;
		} else
			adj		= max(0.0f, down * 4 - 1) * dt * TWEAKS_SCROLL_SPEED;

		fcursor += cont->Down(CBUT_DPAD_UP) ? -adj : adj;
		down	+= dt;

		while (fcursor < 0)
			fcursor += n;
		while (fcursor >= n)
			fcursor -= n;

		cursor = int(fcursor);
	}

	if (focus >= 0 && cont->Down(CBUT_DPAD_LEFT | CBUT_DPAD_RIGHT)) {
		float	adj;
		if (cont->Hit(CBUT_DPAD_LEFT | CBUT_DPAD_RIGHT)) {
			cont->Release(CBUT_DPAD_LEFT | CBUT_DPAD_RIGHT);
			hdown	= 0;
			adj		= 1;
		} else
			adj		= max(0.0f, hdown * 4 - 1) * dt * TWEAKS_ADJUST_SPEED;

		tweak		*t	= GetTweak(root["tweaks"][focus], cursor);
		t->UpdateVal(cont->Down(CBUT_DPAD_LEFT) ? -adj : adj);

		hdown		+= dt;
	}
}

#endif

#if USE_PROFILER

TWEAK(bool, PROFILER_SHOW_MENU, false);
TWEAK(int, PROFILER_STARTING_LINE, 0, 0, 500);

class ProfileUI : public HandlesGlobal<ProfileUI, FrameEvent2>, public HandlesGlobal<ProfileUI, RenderEvent> {
	Profiler::Marker	*open[32];

	struct Printer {
		Profiler::Marker	**open;
		string_accum		&buffer;
		int					index;
		void				Print(const Profiler::Marker *m, const Profiler::Marker *p, int tab);
		Printer(string_accum &_buffer, Profiler::Marker **_open)
			: buffer(_buffer), open(_open), index(0) {}
	};

public:
	void	Display(GraphicsContext &ctx, const Font *font);
	void	Update(Controller *cont, float dt);

	void	operator()(FrameEvent2 &ev) {
		Controller	*cont	= 0;
		if (PROFILER_SHOW_MENU)
			Update(cont, ev.dt);
	}

	void	operator()(RenderEvent &re) {
		if (PROFILER_SHOW_MENU)
			Display(re.ctx, debug_font);
	}

	ProfileUI() {
		clear(open);
		open[0] = &Profiler::instance->head;
	}
} profile_ui;

void ProfileUI::Printer::Print(const Profiler::Marker *m, const Profiler::Marker *p, int tab) {
	if (buffer.remaining() < 1024)
		return;

	bool	selected	= m == open[0] && !open[1];
	bool	expanded	= m == open[0] && open[1];

	if (++index > PROFILER_STARTING_LINE) {

		buffer
			<< cc_colour(selected ? colour(1,1,.5f) : index & 1 ? colour(1,1,0) : colour(1,1,1))
			<< cc_hset(tab * 16)
			<< onlyif(!m->children.empty(), expanded ? '-' : '+');

		if (selected)
			buffer << cc_hset(tab * 16 + 8) << '>';

		Profiler	*prof = Profiler::instance;
		buffer
			<< cc_hset(tab * 16 + 16)
			<< m->Label()
			<< cc_hset(255)
			<< cc_tab << Time::to_secs(m->Total()) * 1000
			<< cc_tab << Time::to_secs(m->RunningTotal()) * 1000 / prof->TotalFrames()
			<< cc_tab << m->NumCalls()				/ prof->TotalFrames()
			<< cc_tab << m->RunningTotal()	* 100	/ prof->head.RunningTotal();

		if (p)
			buffer << cc_tab << m->RunningTotal() * 100 / p->RunningTotal();

		buffer << '\n';
	}

	if (expanded) {
		open++;
		for (Profiler::const_iterator i = m->children.begin(); i != m->children.end(); ++i)
			Print(i, m, tab + 1);
	}
}

void ProfileUI::Display(GraphicsContext &ctx, const Font *font) {
	buffer_accum<4096>	accum;
	accum << cc_hmove(1) << "Marker" << cc_hset(255) << "ms\tavg ms\tx\tavg %\tlocal %\n";
	Printer(accum, open).Print(&Profiler::instance->head, 0, 0);
	DebugPrinter(ctx, font).Print(320, 48, FA_LEFT, accum);
}

void ProfileUI::Update(Controller *cont, float dt) {
	int	depth = 0;
	while (open[depth + 1])
		depth++;
	Profiler::Marker	*cursor = open[depth];

	if (cont->Hit(CBUT_DPAD_RIGHT)) {// go down in the hierarchy
		cont->Release(CBUT_DPAD_RIGHT);
		if (!cursor->children.empty())
			open[depth + 1] = cursor->children.begin(); // set it to the first element

	} else if (cont->Hit(CBUT_DPAD_LEFT)) {// go up in the hierarchy
		cont->Release(CBUT_DPAD_LEFT);
		open[depth] = 0;
		if (!depth)
			::enable_profiler = false;

	} else if (cont->Hit(CBUT_DPAD_UP)) {// go to the previous child
		cont->Release(CBUT_DPAD_UP);
		if (depth && cursor != &open[depth-1]->children.front())
			open[depth] = cursor->prev;

	} else if (cont->Hit(CBUT_DPAD_DOWN)) {// go to the next child
		cont->Release(CBUT_DPAD_DOWN);
		if (depth && cursor != &open[depth-1]->children.back())
			open[depth] = cursor->next;

	} else if (cont->Hit(CBUT_ACTION)) {
		Profiler::instance->Reset();
	}
}

#endif
