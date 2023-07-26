#include "base/vector.h"
#include "main.h"
#include "windows/d2d.h"
#include "iso/iso_convert.h"
#include "iso/iso_binary.h"
#include "base/atomic.h"
#include "systems/communication/connection.h"
#include "stream.h"
#include "packed_types.h"

using namespace app;

class Remote;
const char *GetSpec(Remote *r);

class SI {
	friend size_t to_string(char *s, const SI &si) { return si.process(s); }
	float	v;
	size_t	process(char *s) const;
public:
	SI(float _v) : v(_v)	{}
};

size_t SI::process(char *s) const {
	float	f	= 1000;
	int		u	= 0;
	float	a	= abs(v);
	size_t	len;
	if (a > f) {
		static const char units[] = "kMGTPEZY";
		while (u < num_elements(units) && a > f) {
			f *= 1000;
			u++;
		}
		len = to_string(s, v / f);
		s[len++] = units[u];
	} else if (a < 1 && a != 0) {
		static const char units[] = "m\xBCnpfazy";
		while (u < num_elements(units) && a * f < 1) {
			f *= 1000;
			u++;
		}
		len = to_string(s, v * f);
		if (u == 1)
			s[len++] = '\xCE';
		s[len++] = units[u];
	} else {
		len = to_string(s, v);
	}
	return len;
}


struct ProfileSpan {
	ISO_ptr<void>	p0, p1;

	struct iterator {
		const ProfileSpan	&p;
		int			i;
		iterator(const ProfileSpan &_p, int _i) : p(_p), i(_i) {}
		bool		operator==(const iterator &b)	{ return i == b.i; }
		bool		operator!=(const iterator &b)	{ return i != b.i; }
		iterator	operator++()					{ ++i; return *this; }
		ProfileSpan	operator*()	{
			ISO::Browser	b0(p.p0);
			return ProfileSpan(*b0[i], ISO::Browser(p.p1)[b0.GetName(i)]);
		}
		int			index() const { return i; }
	};

	ProfileSpan() {}
	ProfileSpan(const ISO_ptr<void> &_p0, const ISO_ptr<void> &_p1) : p0(_p0), p1(_p1) {}
	operator	bool()						const	{ return p1; }
	bool operator==(const ProfileSpan &b)	const	{ return p0 == b.p0 && p1 == b.p1; }

	tag			name()		const { return p0.ID(); }
	int			count()		const { return ISO::Browser(p1)["count"].GetInt() - ISO::Browser(p0)["count"].GetInt(); }
	int64		time()		const { return (int64)ISO::Browser(p1)[0].Get(uint64(0)) - (int64)ISO::Browser(p0)[0].Get(uint64(0)); }
	rgba8*		colour()	const { return ISO::Browser(p0)["colour"];	}

	iterator	begin()		const { return iterator(*this, 3); }
	iterator	end()		const { return iterator(*this, max(ISO::Browser(p0).Count(), 3)); }
};

struct DrawColours {
	colour_HSV	hsv0, hsv1;
	DrawColours(const colour_HSV &hsv0, const colour_HSV &hsv1) : hsv0(hsv0), hsv1(hsv1) {}

	colour	GetColour(rgba8 *col = 0) const {
		if (col && *(uint32*)col) {
			return colour(float4(*col));
		} else {
			HDRpixel	col = colour(hsv0).rgba;
			return colour(col.r, col.g, col.b);
		}
	}
	void	GetBrush(d2d::WND &d2d, ID2D1SolidColorBrush **b, rgba8 *col = 0) const {
		d2d.CreateBrush(b, GetColour(col));
	}

	void	GetBrush(d2d::WND &d2d, ID2D1LinearGradientBrush **b, const d2d::point &p0, const d2d::point &p1, rgba8 *col = 0) const {
		d2d::colour	c = GetColour(col);
		d2d::gradstop	g[] = {
			{0.f, c},
			{1.f, colour(one)}
		};
		d2d.CreateBrush(b, p0, p1, g);
	}

	DrawColours	Sub(int i, int n) const {
		float	h0	= lerp(hsv0.h, hsv1.h, float(i + 1) / n);
		float	h1	= lerp(hsv0.h, hsv1.h, float(i + 1) / n);
		float	s	= hsv0.s * 0.75f;
		float	v	= hsv0.v;
		return DrawColours(colour_HSV(h0, s, v), colour_HSV(h1, s, v));
	}
};

struct DrawContext {
	d2d::WND						&d2d;
	d2d::Write						write;
	d2d::SolidBrush					black, white;
	com_ptr<ID2D1SolidColorBrush>	brush[2];
	d2d::Font						font;
	colour_HSV						hsv[2];
	const char						*selected;
	d2d::point						mouse;
	ProfileSpan						hover;

	float Draw(const ProfileSpan &ps, int index, float x, float y, float h, float scale, int depth);
	float DrawBar(const ProfileSpan &ps, int index, float x, float y, float h, float scale, const DrawColours &cols, int depth);
	float DrawBarH(const ProfileSpan &ps, int index, float x, float y, float h, float scale, const DrawColours &cols, int depth);
	float DrawLabels(const ProfileSpan &ps, int index, float x, float y, float w, float scale, const DrawColours &cols, int depth);

	DrawContext(d2d::WND &d2d, const d2d::point &mouse, const char *selected)
		: d2d(d2d)
		, black(d2d, colour(float3(zero), one))
		, white(d2d, colour(one))
		, font(write, L"arial", 10)
		, selected(selected), mouse(mouse)
	{
		d2d.CreateBrush(&brush[0], colour(0.8f,0.8f,0.8f));
		d2d.CreateBrush(&brush[1], colour(0.6f,0.6f,0.6f));
	}
};

float DrawContext::Draw(const ProfileSpan &ps, int index, float x, float y, float h, float scale, int depth) {
	if (!ps)
		return 0;

	float	t	= ps.time() * scale;
	d2d.Fill(rectangle(position2{x, y}, position2{x + t, y + h}), brush[index & 1]);

	if (depth--) {
		y += 25;
		h -= 50;
		for (ProfileSpan::iterator i = ps.begin(), n = ps.end(); i != n; ++i)
			x += Draw(*i, i.index(), x, y, h, scale, depth);
	}
	return t;
}

float DrawContext::DrawBar(const ProfileSpan &ps, int index, float x, float y, float w, float scale, const DrawColours &cols, int depth) {
	if (!ps)
		return 0;

	float		t	= ps.time() * scale;
	rectangle	rect(position2{x, y + t}, position2{x + w, y});
	if (rect.contains(mouse))
		hover = ps;

	if (selected && ps.name() == selected) {
		d2d.Fill(rect, white);
	} else {
		com_ptr<ID2D1SolidColorBrush>	brush;
		cols.GetBrush(d2d, &brush, ps.colour());
		d2d.Fill(rect, brush);
	}

	if (depth--) {
		for (ProfileSpan::iterator i = ps.begin(), n = ps.end(); i != n; ++i) {
			y += DrawBar(*i, i.index(), x + 1, y + 1, w, scale, cols.Sub(i.index(), n.index()), depth);
			hsv[0].h = hsv[1].h;
		}
	}
	return t;
}

float DrawContext::DrawBarH(const ProfileSpan &ps, int index, float x, float y, float h, float scale, const DrawColours &cols, int depth) {
	if (!ps)
		return 0;

	float		t	= ps.time() * scale;
	rectangle	rect(position2{x, y}, position2{x + t, y + h});
	if (rect.contains(mouse))
		hover = ps;

	com_ptr<ID2D1LinearGradientBrush>	brush;
	cols.GetBrush(d2d, &brush, rect.a, rect.b, ps.colour());
	d2d.Fill(rect, brush);
	if (const char *name = ps.name())
		d2d.DrawText(rect, string16(name), font, white, D2D1_DRAW_TEXT_OPTIONS_CLIP);

	if (depth--) {
		for (ProfileSpan::iterator i = ps.begin(), n = ps.end(); i != n; ++i) {
			x += DrawBarH(*i, i.index(), x, y + h, h, scale, cols.Sub(i.index(), n.index()), depth);
			hsv[0].h = hsv[1].h;
		}
	}
	return t;
}

float DrawContext::DrawLabels(const ProfileSpan &ps, int index, float x, float y, float w, float scale, const DrawColours &cols, int depth) {
	if (!ps)
		return 0;

	if (const char *name = ps.name()) {
		com_ptr<ID2D1SolidColorBrush>	brush;
		cols.GetBrush(d2d, &brush, ps.colour());
		d2d.DrawText(rectangle(position2{x, y}, position2{x + w, y - 20}), string16(name), font, brush);
	}

	if (depth--) {
		for (ProfileSpan::iterator i = ps.begin(), n = ps.end(); i != n; ++i)
			y += DrawLabels(*i, i.index(), x + 30, y, w, scale, cols.Sub(i.index(), n.index()), depth);
	}
	return ps.time() * scale;
}

//-----------------------------------------------------------------------------
//	ViewProfile
//-----------------------------------------------------------------------------

class ViewProfile : public aligned<Window<ViewProfile>, 16>, public TimerT<ViewProfile> {
	d2d::WND		d2d;
	MainWindow		&main;
	const char		*target;
	const char		*selected_entry, *hover_entry;
	ProfileSpan		selected_frame, hover_frame;
	ProfileSpan		selected_root, hover_root;
	string			spec;
	float			zoom, tscale;
	float			bar;
	point			pos, prevmouse;
	Rect			client;

	circular_buffer<ISO_ptr<void>*> times;
//	dynamic_array_de<ISO_ptr<void> > times;

	bool CreateDeviceResources() {
		d2d.Init(*this, GetClientRect().Size());
		return true;
	}
	void DiscardDeviceResources() {
		d2d.DeInit();
	}

	static ISO_ptr<void> GetData(const char *target, const char *spec) {
		static atomic<int> reentry;
		ISO_ptr<void>		p;
		if (reentry.cas_acquire(false, true)) {
			uint32be			sizebe;
			isolink_handle_t	handle	= SendCommand(target, ISO_ptr<const char*>("GetAll", spec));

			if (handle != isolink_invalid_handle && isolink_receive(handle, &sizebe, 4)) {
				malloc_block	buffer(sizebe);
				if (isolink_receive(handle, buffer, sizebe) == sizebe)
					p = ISO::binary_data.Read(none, memory_reader(buffer));
				isolink_close(handle);
			}
			reentry = false;
		}
		return p;
	}

public:
	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);

	void	operator()(Timer*) {
		if (ISO_ptr<void> p = GetData(target, spec)) {
			if (times.space() == 0)
				times.pop_front();
			times.push_back(p);

			float	b = bar * zoom;
			float	x = times.size() * b + pos.x + 150;
			if (x >= 0 && x < b * 2)
				pos.x = -int(times.size()) * b - 150;
			Invalidate();
		}
	}

	ViewProfile(MainWindow &_main, const WindowPos &wpos, const ISO_VirtualTarget &b)
		: main(_main), zoom(1), tscale(0.001f)
		, selected_entry(0)
		, bar(10), pos{-150,0}
		, times(new ISO_ptr<void>[1024], 1024)
	{
		target	= ISO::binary_data.RemoteTarget();
		spec	= fixed_string<256>(GetSpec(b.v)) + b.spec;
		Create(wpos, NULL, CHILD | VISIBLE, CLIENTEDGE);
	}
	~ViewProfile() {
		Timer::Stop();
		delete[] times.get_buffer();
	}
};

LRESULT ViewProfile::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
//			Timer::Next(0);
			Timer::Start(0.2f);
			break;

		case WM_SIZE:
			client = GetClientRect();
			if (!d2d.Resize(Point(lParam))) {
				DiscardDeviceResources();
				Invalidate();
			}
			break;

		case WM_PAINT: {
//			Timer::Next(0.2f);
			DeviceContextPaint	dc(*this);
			hover_entry		= 0;
			hover_root		= ProfileSpan();
			hover_frame		= ProfileSpan();
			if (CreateDeviceResources() && !d2d.Occluded()) {
				float2x3	trans	= translate(to<float>(pos + (point)client.BottomRight())) * scale(zoom);// * translate(times.size() * -bar, 0);
				float2x3	itrans	= inverse(trans);

				d2d.BeginDraw();
				d2d.Clear(colour(zero));
				d2d.SetTransform(trans);

				DrawContext	c(d2d, itrans * d2d::point(prevmouse), selected_entry);
				DrawColours	cols(colour_HSV(0,1,1), colour_HSV(1,1,1));
				d2d::point	a	= itrans * d2d::point(client.TopLeft());
				d2d::point	b	= itrans * d2d::point(client.BottomRight() - Point(150,0));
				float		f	= -1e6 / 60 * tscale;

				{// frame bands
					com_ptr<ID2D1SolidColorBrush>	grey[2];
					d2d.CreateBrush(&grey[0], colour(0.5f,0.5f,0.5f));
					d2d.CreateBrush(&grey[1], colour(0.3f,0.3f,0.3f));
					for (int i = max(int(b.y / f), 0), i1 = int(a.y / f); i <= i1; i++)
						d2d.Fill(rectangle(position2{a.x, i * f}, position2{b.x, (i + 1) * f}), grey[i&1]);
				}

				if (times.size() > 1) {
					int			i0	= max(a.x / bar, 1);
					int			i1	= min(b.x / bar + 1, times.size());
					float		range	= 0;
					// bars
					for (int i = i1; i-- > i0; ) {
						ProfileSpan	ps(times[i - 1], times[i]);
						if (int num = ps.count()) {
							float	scale = tscale / num;
							range		= max(range, scale * ps.time());
							c.hover		= ProfileSpan();
							c.selected	= !selected_frame || selected_frame == ps ? selected_entry : 0;
							c.DrawBar(ps, 0, i * bar, 0, 8, -scale, cols, 3);
							if (c.hover) {
								hover_frame		= ps;
								hover_entry		= c.hover.name();
							}
						}
					}

					{// labels
						ProfileSpan	ps(times[max(i1 - 10, 0)], times[max(i1 - 1, 0)]);
						float		scale = tscale / ps.count();
						c.DrawLabels(ps, 0, i1 * bar, 0, 1000, -scale, cols, 3);
					}
					if (range)
						tscale *= pow(500 / range, 0.25f);
				}

				// frame lines
				for (int i = max(int(b.y / f), 0), i1 = int(a.y / f); i <= i1; i++) {
					float	y = i * f;
					d2d.DrawLine(d2d::point(a.x, y), d2d::point(b.x, y), c.white);
				}

				if (times.size() > 1) {
					{// horizontal bars
						ProfileSpan	frame = selected_frame ? selected_frame : ProfileSpan(times[times.size() - 2], times[times.size() - 1]);
						if (int num = frame.count()) {
							d2d.SetTransform(identity);
							c.mouse	= prevmouse;
							c.hover = ProfileSpan();
							ProfileSpan	ps = selected_root ? selected_root : frame;
							c.DrawBarH(ps, 0, 0, 0, 32, client.Width() / float(ps.time()), cols, -1);
							if (hover_root	= c.hover) {
								hover_frame		= frame;

								d2d::point	s(200,100);
								d2d::point	b	= min(c.mouse + s, d2d::point(client.BottomRight()));
								d2d::point	a	= b - s;
								d2d.Fill(d2d::rect(a, b), c.white);

								tag2	name	= hover_root.name();
								float	calls	= float(hover_root.count()) / num;
								float	time	= hover_root.time() / 1e6f;

								d2d.DrawText(rectangle(a + d2d::point(4,  4), b), str16(buffer_accum<256>() << "Name: " << name), c.font, c.black);
								d2d.DrawText(rectangle(a + d2d::point(4, 16), b), str16(buffer_accum<256>() << "Calls: " << calls), c.font, c.black);
								d2d.DrawText(rectangle(a + d2d::point(4, 28), b), str16(buffer_accum<256>() << "Time: " << SI(time) << 's'), c.font, c.black);
							}
						}
					}
				}

				if (d2d.EndDraw())
					DiscardDeviceResources();
			}
			break;
		}
		case WM_LBUTTONDOWN:
			selected_frame	= hover_frame;
			if (hover_root) {
				if (hover_root == selected_root)
					selected_root = ProfileSpan();
				else
					selected_root = hover_root;
			} else {
				selected_entry	= hover_entry;
			}
			SetFocus();
			break;

		case WM_MOUSEMOVE: {
			point	mouse	= Point(lParam);
			if (wParam & MK_LBUTTON) {
				pos			+= mouse - prevmouse;
				Invalidate();
			}
			prevmouse	= mouse;
			break;
		}
		case WM_MOUSEWHEEL: {
			point	pt		= ToClient(Point(lParam)) - client.BottomRight();
			float	mult	=iso::pow(1.05f, (short)HIWORD(wParam) / 64.f);
			zoom	*= mult;
			pos		= pt + to<int>(to<float>(pos - pt) * mult);
			Invalidate();
			break;
		}

		case WM_COMMAND:
			if (LOWORD(wParam) == ID_EDIT_SELECT) {
				ISO::Browser b	= *(ISO::Browser*)lParam;
				const ISO::Type	*type = b.GetTypeDef();
				if (type->SkipUser()->GetType() == ISO::REFERENCE) {
					type	= (*b).GetTypeDef();
					if (type->Is("ProfileData"))
						selected_entry = tag(b.GetName());
				}
			}
			break;

		case WM_NOTIFY:
			break;

		case WM_NCDESTROY:
			delete this;
			break;
		default:
			return Super(message, wParam, lParam);
	}
	return 0;
}


class EditorProfile : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->Is("Profiler");
//		return type->Is("ProfileData");
	}
	virtual Control Create(MainWindow &main, const WindowPos &pos, const ISO_ptr<void> &p) {
		return Control();//*new ViewProfile(main, rect, p);
	}
	virtual Control	Create(MainWindow &main, const WindowPos &wpos, const ISO_VirtualTarget &b) {
		return *new ViewProfile(main, wpos, b);
	}

} editorprofile;
