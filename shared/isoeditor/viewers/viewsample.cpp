#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "windows/d2d.h"
#include "windows/filedialogs.h"
#include "windows/control_helpers.h"
#include "sound.h"
#include "viewer.h"
#include "viewsample.rc.h"
#include "extra/si.h"

#define	IDR_TOOLBAR_SAMPLE	"IDR_TOOLBAR_SAMPLE"

using namespace app;

Sound	sound;

class ViewSample : public Window<ViewSample>, public WindowTimer<ViewSample> {
	ToolTipControl		tooltip;
	ToolBarControl		toolbar;
	bool				tip_on;
	ISO_ptr_machine<sample>		sm;
	SoundBuffer			*sb;
	SoundVoiceHolder	voice;
	float				xscale, yscale;
	uint32				cursor;
	Point				pos;
	Point				prevmouse;

#ifdef D2D_H
	d2d::WND			target;
	d2d::Write			write;
#endif

	static filename	fn;

	int		GetLocation(const Point &pt, int *chan) {
		int		s	= (pt.x - pos.x) / xscale;
		if (s < 0 || s >= sm->Length())
			return -1;

		int		n	= sm->Channels();
		float	h	= GetClientRect().Height();
		float	c	= pt.y * n / h;
		if (abs((frac(c) - 0.5f)) * h / n >= 32768 * yscale)
			return -1;

		if (chan)
			*chan	= int(c);

		return s;
	}
	float	SampleToClient(int s)	const	{ return s * xscale + pos.x; }
	float	ClientToSample(int x)	const	{ return (x - pos.x) / xscale; }
	float	ClientToTime(int x)		const	{ return ClientToSample(x) / sm->Frequency(); }
	int		TimeToClient(float t)	const	{ return SampleToClient(t * sm->Frequency()); }

	bool	GetTipText(string_accum &acc, const Point &pt) {
		int		c, s = GetLocation(pt, &c);
		if (s < 0)
			return false;

		acc.format("@%i (%fs): chan[%i] = %i", s, s / sm->Frequency(), c, sm->Samples()[s * sm->Channels() + c]);
		return true;
	}
	void	SetCursor(uint32 new_cursor) {
		uint32	h = GetClientRect().Height();
		Invalidate(Rect(SampleToClient(cursor) - 1, 0, 2, h));
		cursor = new_cursor;
		Invalidate(Rect(SampleToClient(cursor) - 1, 0, 2, h));
	}
public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);
	void	Paint(const Rect &dirty);

	ViewSample(const WindowPos &wpos, ISO_ptr_machine<void> p, ToolBarControl _toolbar) : toolbar(_toolbar), sb(0), xscale(0), pos(0,0), tip_on(false), cursor(0) {
		sm = ISO_conversion::convert<sample>(p);

		sound.Init();
		Create(wpos, tag(sm.ID()), CHILD | CLIPCHILDREN, CLIENTEDGE);
	}
};

LRESULT ViewSample::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			tooltip.Create(*this, NULL, POPUP);// | TTS_NOPREFIX | TTS_ALWAYSTIP);
			tooltip.Add(*this);
			Timer::Start(1 / 60.f);
			break;

		case WM_SIZE: {
			Point	size(lParam);
			if (xscale == 0)
				xscale = float(size.x) / sm->Length();
			yscale	= float(size.y) / ((sm->Channels() + 1) * 65536);
#ifdef D2D_H
			if (target.Resize(size))
				target.DeInit();
#endif
			Invalidate();
			break;
		}

		case WM_PAINT: {
			DeviceContextPaint	dc(*this);
			Rect	client	= GetClientRect();
			if (target.Init(hWnd, client.Size()) && !target.Occluded()) {
				target.BeginDraw();
				Paint(dc.GetDirtyRect());
				if (target.EndDraw())
					target.DeInit();
			}
			return 0;
		}

		case WM_MOUSEWHEEL:
			if (GetRect().Contains(Point(lParam))) {
				Point	pt		= ToClient(Point(lParam));
				float	mult	= pow(1.05f, (short)HIWORD(wParam) / 64.f);
				xscale	*= mult;
				pos.x	= pt.x + mult * (pos.x - pt.x);
				pos.y	= pt.y + mult * (pos.y - pt.y);

				Invalidate();
			}
			break;

		case WM_LBUTTONDOWN:
		case WM_RBUTTONDOWN:
			prevmouse	= Point(lParam);
			SetFocus();
			break;

		case WM_MOUSEMOVE: {
			Point mouse(lParam);

			if (wParam & MK_LBUTTON) {
				pos			+= mouse - prevmouse;
				//Scroll(mouse.x - prevmouse.x, 0, SW_INVALIDATE, &rects[1]);
				prevmouse	= mouse;
				Invalidate();
			}

			if (!tip_on && GetLocation(mouse, NULL) >= 0) {
				TRACKMOUSEEVENT	tme = {sizeof(TRACKMOUSEEVENT), TME_LEAVE, hWnd, HOVER_DEFAULT};
				TrackMouseEvent(&tme);
				tooltip.Activate(*this, tip_on = true);
			}
			tooltip.Track(GetMousePos() + Point(15, 15));
			break;
		}

		case WM_MOUSELEAVE:
			tooltip.Activate(*this, tip_on = false);
			break;

		case WM_ISO_TIMER:
			sound.Update(0, identity);
			if (voice) {
				if (voice->IsPlaying()) {
					SetCursor(voice->GetPlayPosition() / sm->BytesPerSample());

				} else {
					toolbar.CheckButton(ID_SAMPLE_PLAY, false);
					voice.clear();
				}
			}
			break;

		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
				case ID_SAMPLE_PLAY:
					if (voice) {
						bool	paused = voice->IsPaused();
						voice->Pause(!paused);
						ToolBarControl::Item().Image(paused ? 3 : 0).Set(toolbar, id);
						toolbar.Invalidate();
						//toolbar.CheckButton(id, state);
					} else {
						if (!sb)
							sb = new SoundBuffer(sm);
						voice = Play(sb);
						ToolBarControl::Item().Image(3).Set(toolbar, id);
						toolbar.Invalidate();
						//toolbar.CheckButton(id);
					}
					break;

//				case ID_SAMPLE_END:
				case ID_SAMPLE_STOP:
					if (voice) {
						voice.Stop();
						toolbar.CheckButton(ID_SAMPLE_PLAY, false);
						toolbar.CheckButton(ID_SAMPLE_PAUSE, false);
					}
					break;

				case ID_SAMPLE_PAUSE:
					if (voice) {
						bool	state = !voice->IsPaused();
						voice->Pause(state);
						toolbar.CheckButton(id, state);
					}
					break;

				case ID_SAMPLE_SAVEAS: {
					buffer_accum<1024>	ba;
					for (FileHandler::iterator i = FileHandler::begin(); i != FileHandler::end(); ++i) {
						if (i->GetCategory() == cstr("sample")) {
							if (const char *des = i->GetDescription()) {
								ba.format("%s (*.%s)", des, i->GetExt()) << "\0";
								ba.format("*.%s", i->GetExt()) << "\0";
							}
						}
					}
					//if (GetSave(hWnd, fn, "Save As", "Sample Files\0*.wav;*.mp3;*.aif\0All Files (*.*)\0*.*\0"))
					if (GetSave(hWnd, fn, "Save As", ba))
						Busy(), FileHandler::Write(sm, fn);
					break;
				}

			}
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TTN_GETDISPINFOA:
					if (nmh->hwndFrom == tooltip) {
						if (!GetTipText(lvalue(fixed_accum(((NMTTDISPINFOA*)nmh)->szText)), ToClient(GetMousePos())))
							tooltip.Activate(*this, tip_on = false);
					} else {
						TOOLTIPTEXT	*ttt	= (TOOLTIPTEXT*)nmh;
						ttt->hinst			= GetLocalInstance();
						ttt->lpszText		= MAKEINTRESOURCE(ttt->hdr.idFrom);
					}
					break;
			}
			break;
		}
		case WM_NCDESTROY:
			toolbar.Destroy();
			tooltip.Destroy();
			delete this;
			break;

		default:
			return Super(message, wParam, lParam);
	}
	return 0;
}

void ViewSample::Paint(const Rect &dirty) {
	Rect	client	= GetClientRect();
	int16*	samples	= sm->Samples();
	int		nchans	= sm->Channels();
	float	rscale	= 1 / xscale;
	int		nsamps	= int(ceil(rscale));
	int		minx	= max(dirty.left, pos.x), maxx = min(float(dirty.right), sm->Length() * xscale + pos.x);

	target.SetTransform(identity);

	d2d::SolidBrush	white(target, colour(1,1,1)), black(target, colour(0,0,0)), blue(target, colour(.8f,.8f,1));
	target.Fill(dirty, white);
	d2d::Clipped	clipped(target, dirty);

	float	lscale	= 2 - log10(xscale * sm->Frequency());
	float	tscale	= pow(10.f, int(floor(lscale)));
//	float	tscale	= xscale * sm->Frequency();
//	float	time	= client.Width() / tscale;
//	float	units	= pow(10.f, floor(log10(time) - 0.5f));
//	tscale *= units;

	{// times
		float	fade = ceil(lscale) - lscale;
		d2d::Font	font(write, L"arial", 10);
		com_ptr<ID2D1SolidColorBrush>	textbrush[2];

		target.CreateBrush(&textbrush[0],	colour(max(fade * 2 - 1, 0)));
		target.CreateBrush(&textbrush[1],	colour(0,0,0, 1));

		com_ptr<ID2D1SolidColorBrush>	linebrush[2];
		target.CreateBrush(&linebrush[0],	colour(fade * 0.75f));
		target.CreateBrush(&linebrush[1],	colour(0.75f));

		float	tleft	= ClientToTime(0);
		float	tright	= ClientToTime(client.Width());

		char	si_suffix[4];
		float	si_scale = tscale / SIsuffix(max(abs(tleft), abs(tright)), si_suffix);

		for (int i = int(floor(ClientToTime(minx) / tscale)), i1 = int(ClientToTime(maxx) / tscale); i <= i1; i++) {
			float	x		= TimeToClient(i * tscale);
			bool	mul10	= (abs(i) % 10) == 0;
			target.DrawLine(d2d::point(x, 0), d2d::point(x, 10), linebrush[mul10]);
			if (mul10 || fade > 0.5f)
				target.DrawText(d2d::rect(x, 0, x + 100, 10), str16(buffer_accum<256>() << i * si_scale << (char*)si_suffix << 's'), font, textbrush[mul10], D2D1_DRAW_TEXT_OPTIONS_CLIP);
		}
	}

	for (int c = 0; c < nchans; c++) {
		int		middle	= client.Top() + client.Height() * (2 * c + 1) / (2 * nchans);
		if (middle - 32768 * yscale > dirty.bottom || middle + 32768 * yscale < dirty.top)
			continue;

		Rect	rect(minx, middle - 32768 * yscale, maxx - minx, 65536 * yscale);
		target.Fill(rect, blue);
		target.DrawLine(d2d::point(minx, middle), d2d::point(maxx, middle), black);

#if 0
		com_ptr<ID2D1PathGeometry>	geom;
		com_ptr<ID2D1GeometrySink>	sink;
		target.CreatePath(&geom, &sink);
		sink->BeginFigure(d2d::point(minx, middle), D2D1_FIGURE_BEGIN_HOLLOW);
#endif
		for (int x = minx; x < maxx; x++) {
			int16	*p	= samples + int((x - pos.x) * rscale) * nchans + c;
			int16	a	= 0, b = 0;
			for (int i = 0; i < nsamps; i++, p += nchans) {
				a = min(a, *p);
				b = max(b, *p);
			}
#if 0
			sink->AddLine(d2d::point(x, a * yscale));
#else
			target.DrawLine(
				d2d::point(x, middle - a * yscale),
				d2d::point(x, middle - b * yscale),
				black
			);
#endif
		}
#if 0
		sink->EndFigure(D2D1_FIGURE_END_OPEN);
		target.Fill(geom, black);
#endif
	}

	float	x = SampleToClient(cursor);
	target.DrawLine(d2d::point(x, 0), d2d::point(x, client.Height()), black);
}

filename ViewSample::fn;

class EditorSample : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->SameAs<sample>()
			|| type->Is("SampleBuffer");
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr_machine<void> &p) {
		ToolBarControl	tb(main, NULL, Control::CHILD | CCS_NODIVIDER | CCS_NORESIZE | TBSTYLE_FLAT | TBSTYLE_TRANSPARENT | TBSTYLE_TOOLTIPS);
		tb.Init(GetLocalInstance(), IDR_TOOLBAR_SAMPLE);
		main.AddToolbar(tb);
		return *new ViewSample(wpos, FileHandler::ExpandExternals(p), tb);
	}
} editorsample;
