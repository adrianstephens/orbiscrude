#include "main.h"
#include "graphics.h"
#include "iso/iso_convert.h"
#include "base/tree.h"
#include "windows/dib.h"
#include "systems/ui/font.h"
#include "systems/ui/menu.h"
#include "systems/Particle\particle.h"
#include "viewmenu.rc.h"

#define IDR_MENU_MENU	"IDR_MENU_MENU"

using namespace app;

struct {
	int	w, h;
} coord_systems[] = {
	{1280,	720},
	{960,	720},
	{1920,	1080},
	{1440,	1080},
	{0,		0}
};

float	eye_sep		= 25.f;
float	eye_focus	= 100.f;

float3x3 skipz(const float4x4 &m) {
	return float3x3(m.x.xyw, m.y.xyw, m.w.xyw);
}

class ViewMenu : public aligned<Window<ViewMenu>, 16>, public WindowTimer<ViewMenu>, public Graphics::Display {
#ifdef ISO_EDITOR
	typedef IsoEditor	MainWindowType;
#else
	typedef MainWindow	MainWindowType;
#endif
	static int							keys[8];
	static ISO::WeakPtr<void>			textlookup;
	static Texture						temporary;
	static ControlArrangement::Token	arrange[];

	ToolBarControl		toolbar;
	Menu				menu;
	Rect				rects[2];
	ISO_ptr<Texture>	overlay;
	float				overlay_alpha		= 0.4f;
	void				*currentcxt;// just to check we're on the right selection
	ISO_ptr<void>		p;
	ISO_ptr<void>		currentsel, prevsel;
	float4x4			currentspace, postspace;
	float2				currentsize, postsize;
	int					subselection		= -1;
	int					backup = 0, depth	= 0;
	int					coord_system		= 0;
	float				speed				= 1;
	bool				draw				= true;
	bool				pointer				= false;
	bool				anaglyph			= false;

	MainWindowType		&main;
	timer				time;
	MenuInstance		mi;
	Point				mouse;
	float2				fmouse, fmouse_click;
	uint32				buttons = 0, prevbuttons, hitbuttons;
	float				time_last, time_offset	= 0;

	struct mi_box {
		float	x, y, w, h;
		float	scale;
	};

	struct mi_warp {
		float	c[4][2];
	};

	struct mi_offset {
		float	x, y;
		float	scale;
	};

	struct mi_centre {
		float	w, h;
	};

	ModifyOp	*currentop = nullptr;

	static float addc(float v, float m) {
		return v < 0 ? min(v + m, 0.f) : max(v + m, 0.f);
	}

	void	SetSpeed(float new_speed) {
		time_offset	+= mi.GetTime() * (speed - new_speed);
		speed		= new_speed;
	}

public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);
	intptr_t	operator()(MenuInstance *mi, tag id, MENU_MESSAGE msg, void *params);

	ViewMenu(MainWindow &_main, const WindowPos &wpos, const ISO_ptr<void> &_p) : p(_p), main((MainWindowType&)_main), mi(this) {
		mi.Init(p, 0);
		Create(wpos, NULL, CHILD | VISIBLE | CLIPCHILDREN, CLIENTEDGE);
		Timer::Start(1.f/20);

		if (!temporary)
			(ISO_ptr<void>&)temporary = ISO::root("data")["temporary"];
	}
};

enum {
	CBUT_DPAD_LEFT	= 1 << 0,
	CBUT_DPAD_RIGHT	= 1 << 1,
	CBUT_DPAD_UP	= 1 << 2,
	CBUT_DPAD_DOWN	= 1 << 3,
	CBUT_ACTION		= 1 << 4,
	CBUT_RETURN		= 1 << 5,
	CBUT_ACTION2	= 1 << 6,
	CBUT_ACTION3	= 1 << 7,
	CBUT_PTR		= 1 << 8,
};

int ViewMenu::keys[] = {
	VK_LEFT,
	VK_RIGHT,
	VK_UP,
	VK_DOWN,
	'A',//VK_SPACE,
	'B',//VK_BACK,
	'X',
	'Y',
};

ISO::WeakPtr<void>	ViewMenu::textlookup;
Texture				ViewMenu::temporary;
ControlArrangement::Token ViewMenu::arrange[] = {
	ControlArrangement::VSplit(29),
	ControlArrangement::ControlRect(0),
	ControlArrangement::ControlRect(1),
};

#ifndef USE_DX11
template<typename T> struct BlockD3D : LockedRect, block<T, 2> {
	BlockD3D(const Texture &tex) : LockedRect(tex.Data()), block<T, 2>((T*)pBits, Pitch / sizeof(T), tex.Width(), tex.Height()) {}
};
#endif

LRESULT ViewMenu::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE:
			menu	= Menu(IDR_MENU_MENU);
			toolbar.Create(*this, NULL, CHILD | VISIBLE | CLIPSIBLINGS | TBSTYLE_FLAT);
			toolbar.Init(menu);
			menu.Radio(ID_MENU_SETCOORDINATESYSTEM_1280X720);
			menu.Radio(ID_MENU_SETSPEED_NORMAL);
			break;

		case WM_SIZE:
			ControlArrangement::GetRects(arrange, Rect(Point(0, 0), Point(lParam)), rects);
			toolbar.Move(rects[0]);
			SetSize((RenderWindow*)hWnd, rects[1].Size());
			break;

		case WM_PAINT: {
			static bool painting = false;
			if (painting)
				break;

			painting = true;

			DeviceContextPaint	dc(*this);
			Rect&		client	= rects[1];

			GraphicsContext ctx;
			graphics.BeginScene(ctx);
			ctx.SetRenderTarget(GetDispSurface());
			ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);
			ctx.SetDepthTestEnable(false);

			float	w	= client.Width(), h = client.Height(), x0, y0;
			float	w1	= coord_systems[coord_system].w, h1 = coord_systems[coord_system].h;

			if (w1 == 0)
				w1 = w;
			if (h1 == 0)
				h1 = h;

			if (w / w1 < h / h1) {
				x0 = 0;
				y0 = (h * w1 / w - h1) / 2;
			} else {
				y0 = 0;
				x0 = (w * h1 / h - w1) / 2;
			}

			float4x4		proj	= parallel_projection_rect(-(client.left + x0), w1 + x0, -(client.top + y0), h1 + y0, 0.f, 0.f);
			RenderRegion	rr(ctx, proj, w1, h1);
			mi.SetTexture(&temporary);

			if (w1 == w && h1 == h) {
				ctx.Clear(colour(0.5f,0.5f,0.5f));
			} else {
				ctx.Clear(colour(0,0,0));
				rr.Fill(colour(0.5f,0.5f,0.5f));
			}

			if (draw) try {
				Point		m	= ToClient(GetMousePos()) - client.TopLeft();
				fmouse			= float2{m.x / w * 2 - 1, m.y / h * 2 - 1};
				m				= mouse - client.TopLeft();
				fmouse_click	= float2{m.x / w * 2 - 1, m.y / h * 2 - 1};
				saver<bool>	sp(pointer, pointer && !backup);

				if (backup)
					currentsel	= ISO_NULL;

				if (anaglyph) {
					ctx.SetMask(CM_RED);
					rr.matrix = proj * stereo_skew(-eye_sep / 2, eye_focus);
					mi.Render(rr);
					ctx.SetMask(CM_GREEN | CM_BLUE);
					rr.matrix = proj * stereo_skew(+eye_sep / 2, eye_focus);
					mi.Render(rr);
					ctx.SetMask(CM_ALL);
				} else {
					mi.Render(rr);
				}

				if (overlay)
					rr.SetField(REGION_ALPHA, overlay_alpha).Draw(*overlay, rr.technique, ISO::Browser(), 0);

				if (backup) {
					depth	-= backup;
					backup	= depth;
				}
			} catch (const char *s) {
				draw = false;
				MessageBoxA(*this, s, "Error", MB_OK);
			}
			graphics.EndScene(ctx);
			Present();

#ifdef ISO_EDITOR
			if (currentsel && (void*)currentsel != (void*)prevsel)
				main.Select(prevsel = currentsel);
#endif
			painting = false;
			break;
		}
		case WM_LBUTTONDOWN:
			SetFocus();
			buttons			|= CBUT_PTR;
			mouse			= Point(lParam);
			if (wParam & MK_SHIFT) {
				backup		= ++depth;
			} else if (wParam & MK_CONTROL) {
				backup		= --depth;
			} else if (currentsel && subselection < 0) {
				backup		= depth = 1;
				currentsel	= ISO_NULL;
			}
			break;

		case WM_LBUTTONUP:
			buttons		&= ~CBUT_PTR;
			currentop	= NULL;
			break;

		case WM_RBUTTONDOWN:
			SetFocus();
#ifdef ISO_EDITOR
			if (currentsel)
				main.AddEntry(currentsel);
#endif
			break;

//		case WM_CONTEXTMENU:
//			Menu(IDR_MENU_SUBMENUS).GetSubMenu("Menu").Track(*this, Point(lParam), TPM_NONOTIFY | TPM_RIGHTBUTTON);
//			break;

		case WM_MOUSEMOVE: {
			if (!currentsel)
				break;

			Rect&		client		= rects[1];
			float3x4	clientspace	= translate(float3{client.Left(), client.Top(), 0}) * scale(float3{client.Width() / 2, client.Height() / 2, 1}) * translate(position3(1,1,0));
			float3x3	space		= skipz(clientspace * currentspace);
			float3x3	space2		= skipz(clientspace * postspace);

			if ((wParam & MK_LBUTTON) && subselection >= 0) {
				Point	mouse2(lParam);
				Point	move	= mouse2 - mouse;

				if (move.x || move.y) {

#ifdef ISO_EDITOR
					if (!currentop)
						main.Do(currentop = new ModifyOp(currentsel));
#endif
					float2	movev	= project(float3{float(mouse2.x), float(mouse2.y), one} / space)
									- project(float3{float(mouse.x),  float(mouse.y),  one} / space);

					if (currentsel.IsType("mi_box")) {
						mi_box *box = currentsel;
						if (wParam & MK_SHIFT) {
							box->scale *= pow(1.01f, -(float)move.y);
						} else if (subselection == 0) {
							box->x  = addc(box->x, movev.x);
							box->y  = addc(box->y, movev.y);
						} else {
							if (subselection & 4) {
								if (subselection & 1) {
									box->w  = addc(box->w, movev.x);
								} else {
									box->x  = addc(box->x, movev.x);
									if (box->w > 0)
										box->w = addc(box->w, -movev.x);
								}
							}
							if (subselection & 8) {
								if (subselection & 2) {
									box->h  = addc(box->h, movev.y);
								} else {
									box->y  = addc(box->y, movev.y);
									if (box->h > 0)
										box->h = addc(box->h, -movev.y);
								}
							}
						}
					} else if (currentsel.IsType("mi_warp")) {
						mi_warp *warp = currentsel;
						int	p0 = subselection & 3;
						warp->c[p0][0] += movev.x;
						warp->c[p0][1] += movev.y;
						switch (subselection & 12) {
							case 0:
								warp->c[1][0] += movev.x;
								warp->c[1][1] += movev.y;
								warp->c[3][0] += movev.x;
								warp->c[3][1] += movev.y;
							case 4: p0 ^= 3;
							case 8: p0 ^= 1;
								warp->c[p0][0] += movev.x;
								warp->c[p0][1] += movev.y;
							default:
								break;
						}
					} else if (currentsel.IsType("mi_offset")) {
						mi_offset *off = currentsel;
						if (wParam & MK_SHIFT) {
							off->scale *= pow(1.01f, -(float)move.y);
						} else {
							off->x += move.x;
							off->y += move.y;
						}
					} else if (currentsel.IsType("mi_centre")) {
						mi_centre *centre = currentsel;
						centre->w += movev.x;
						centre->h += movev.y;
					}
				}
				mouse = mouse2;
#ifdef ISO_EDITOR
				main.Update(currentsel);
#endif

			} else {
				mouse			= Point(lParam);

				float2	mousev	= project(float3{float(mouse.x), float(mouse.y), one} / space2);
				float2	slop	= project(float3{float(mouse.x) + 4, float(mouse.y) + 4, one} / space2) - mousev;
				if (mousev.x > -slop.x && mousev.x < postsize.x + slop.x && mousev.y > -slop.y && mousev.y < postsize.y + slop.y) {
					subselection = 0;
					if (abs(mousev.x) < slop.x)
						subselection |= 4;
					else if (abs(mousev.x - postsize.x) < slop.x)
						subselection |= 5;

					if (abs(mousev.y) < slop.y)
						subselection |= 8;
					else if (abs(mousev.y - postsize.y) < slop.y)
						subselection |= 10;
				} else {
					subselection	= -1;
				}

			}
			if (subselection >= 0) {
				const static LPTSTR	cursors[] = {
					IDC_SIZEALL,	0,				0,				0,
					IDC_SIZEWE,		IDC_SIZEWE,		0,				0,
					IDC_SIZENS,		0,				IDC_SIZENS,		0,
					IDC_SIZENWSE,	IDC_SIZENESW,	IDC_SIZENESW,	IDC_SIZENWSE,
				};
				SetCursor(LoadCursor(NULL, cursors[subselection]));
			}

			break;
		}

		case WM_KEYDOWN:
			for (int i = 0; i < num_elements(keys); i++) {
				if (keys[i] == int(wParam)) {
					buttons |= 1 << i;
					break;
				}
			}
			break;

		case WM_KEYUP:
			for (int i = 0; i < num_elements(keys); i++) {
				if (keys[i] == int(wParam)) {
					buttons &= ~(1 << i);
					break;
				}
			}
			break;

		case WM_ISO_TIMER:
			hitbuttons	= buttons & ~prevbuttons;
			prevbuttons	= buttons;
			{
				float	t0	= mi.GetTime();
				float	t1	= float(time) * speed + time_offset;
				time_last	= t0;
				mi.SetTime(t1);
			}
			mi.Update();
			Invalidate();
			break;

		case WM_COMMAND:
			switch (int id = LOWORD(wParam)) {
#ifdef ISO_EDITOR
				case ID_EDIT_SELECT: {
					ISO::Browser b = main.GetSelection();
					if (b.SkipUser().GetType() == ISO::REFERENCE) {
						b = *b;
						if (b.Is("mi_box") || b.Is("mi_offset") || b.Is("mi_warp") || b.Is("mi_centre")) {
							currentsel = b;
							currentcxt = 0;
							return TRUE;
						} else if (b.Is("menu") && lParam) {
							ISO_ptr<void>	p2 = ISO_conversion::convert(b, b.GetTypeDef(), ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK | ISO_conversion::EXPAND_EXTERNALS);
							mi.Init(p2, float(time) * speed + time_offset);
							p			= p2;
							currentsel	= ISO_NULL;
							draw		= true;
							depth		= 0;
							return TRUE;
						}
					}
					break;
				}
				case ID_MENU_SETTEXTLOOKUP:
					textlookup = *main.GetSelection();
					break;
				case ID_MENU_SETOVERLAYBITMAP:
					overlay = ISO_conversion::convert<Texture>(*main.GetSelection());
					break;
#endif

				case ID_MENU_SETCOORDINATESYSTEM_1280X720:	coord_system = 0;	menu.Radio(id); break;
				case ID_MENU_SETCOORDINATESYSTEM_960X720:	coord_system = 1;	menu.Radio(id); break;
				case ID_MENU_SETCOORDINATESYSTEM_1920X1080:	coord_system = 2;	menu.Radio(id); break;
				case ID_MENU_SETCOORDINATESYSTEM_1440X1080:	coord_system = 3;	menu.Radio(id); break;
				case ID_MENU_SETCOORDINATESYSTEM_WINDOW:	coord_system = 4;	menu.Radio(id); break;

				case ID_MENU_SETSPEED_STOP:					SetSpeed(0);		menu.Radio(id); break;
				case ID_MENU_SETSPEED_NORMAL:				SetSpeed(1);		menu.Radio(id); break;
				case ID_MENU_SETSPEED_FASTX10:				SetSpeed(10);		menu.Radio(id); break;
				case ID_MENU_SETSPEED_SLOWX10:				SetSpeed(0.1f);		menu.Radio(id); break;
				case ID_MENU_SETSPEED_SLOWX100:				SetSpeed(0.01f);	menu.Radio(id); break;

				case ID_MENU_POINTERSELECTION:				menu.CheckByID(id, pointer = !pointer);		break;
				case ID_MENU_ANAGLYPH:						menu.CheckByID(id, anaglyph = !anaglyph);	break;


#ifndef USE_DX11
				case ID_EDIT_COPY:
					try {
						GraphicsContext ctx;
						graphics.BeginScene(ctx);

						float	w	= coord_systems[coord_system].w, h = coord_systems[coord_system].h;
						if (w == 0)
							w = rects[1].Width();
						if (h == 0)
							h = rects[1].Height();
						float4x4		proj	= parallel_projection_rect(0, w, 0, h, 0, 0);
						RenderRegion	rr(ctx, proj, w, h);
						mi.SetTexture(&temporary);

						Texture		disp(TEXF_R8G8B8A8, w, h, 1, 1);
						ctx.SetRenderTarget(disp);
						ctx.Clear(colour(0.5f,0.5f,0.5f));

						ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);
						ctx.SetDepthTestEnable(false);

						mi.Render(rr);
						graphics.EndScene(ctx);

						Clipboard	clip(*this);
						if (clip.Empty()) {
							Texture		disp2(TEXF_R8G8B8A8, w, h, 1, 1, MEM_SYSTEM);
							ctx.Device()->GetRenderTargetData(disp.GetSurface(0), disp2.GetSurface(0));

							Global	glob(DIB::CalcSize(w, h));
							DIB::Create(glob.Temp(), w, h)->SetPixels(BlockD3D<Texel<B8G8R8A8> >(disp2));
							clip.Set(CF_DIB, glob);
						}
					} catch (const char *s) {
						MessageBoxA(*this, s, "Error", MB_OK);
					}
					break;
#endif
			}
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TBN_DROPDOWN: {
					NMTOOLBAR	*nmtb	= (NMTOOLBAR*)nmh;
					ToolBarControl(nmh->hwndFrom).GetItem(nmtb->iItem).Param<Menu>().Track(*this, ToScreen(Rect(nmtb->rcButton).BottomLeft()));
					break;
				}
			}
			break;
		}

		case WM_NCDESTROY:
			delete this;
			break;
		default:
			return Super(message, wParam, lParam);
	}
	return 0;
}


intptr_t ViewMenu::operator()(MenuInstance *mi, tag id, MENU_MESSAGE msg, void *params) {
	struct node : e_rbnode<node, false> {
		tag	id;
		int	i;
		node(tag _id) : id(_id), i(0)	{}
		operator const char*()	const	{ return id; }
//		bool operator<(const node &n2)	{ return id < n2.id; }
	};
	static e_rbtree<node>	tree;

	switch (msg) {
		case MMSG_GETINTPARAMS:
			if (MenuIntVals *mparams = (MenuIntVals*)params) {
				if (ISO::Browser b = mi->GetBrowser()[id]) {
					mparams->Set(b, 0, 100);
				} else {
					auto	i	= find(tree, id);
					node	*n	= i;
					if (!n)
						tree.insert(i, n = new node(id));
					mparams->minval = -100;
					mparams->maxval	= +100;
					mparams->pval	= &n->i;
					return 1;
				}
			}
			break;

		case MMSG_GETINDEXPARAMS:
			if (MenuIndexVals *mparams = (MenuIndexVals*)params) {
				if (ISO::Browser b = mi->GetBrowser()[id]) {
					mparams->index = b;
				} else {
					auto	i	= find(tree, id);
					node	*n	= i;
					if (!n)
						tree.insert(i, n = new node(id));
					mparams->index	= &n->i;
					return 1;
				}
			}
			break;

		case MMSG_GETSTRING: {
#if 0
			if (params)
				break;
			if (textlookup) {
				if (ISO::Browser t = ISO::Browser(textlookup)[id]) {
					if (t.GetType() == ISO::ARRAY || t.GetType() == ISO::OPENARRAY)
						t = t[0];
					if (const char *s = t.GetString())
						return (int)s;
				}
			}
			return (int)(const char*)id;
#else
			if (!params) {
				const char	*s;
				if (textlookup && (s = ISO::Browser(ISO_ptr_machine<void>(textlookup))[id].GetString()) && strlen(s) > 1023)
					return intptr_t(s);
				break;
			}
			char	*buffer = (char*)params;
			if (textlookup) {
				if (ISO::Browser t = ISO::Browser(ISO_ptr_machine<void>(textlookup))[id]) {
					if (t.GetType() == ISO::ARRAY || t.GetType() == ISO::OPENARRAY)
						t = t[0];
					if (const char *s = t.GetString()) {
						if (strlen(s) > 1023)
							s = "string too long";
						return intptr_t(strcpy(buffer, s));
					}
				}
			}
			if (id == "id_index")
				return (fixed_accum(buffer, 256) << mi->GetIndex() + 1), 1;
			return intptr_t(strcpy(buffer, id));
#endif
		}

		case MMSG_INPUTUPDATE: {
			int	ret;
			if (pointer && (hitbuttons & CBUT_PTR))
				ret = MIU_TRIGGER;
			else if (hitbuttons & CBUT_DPAD_LEFT)
				ret = MIU_HITLEFT;
			else if (hitbuttons & CBUT_DPAD_RIGHT)
				ret = MIU_HITRIGHT;
			else if (hitbuttons & CBUT_DPAD_UP)
				ret = MIU_HITUP;
			else if (hitbuttons & CBUT_DPAD_DOWN)
				ret = MIU_HITDOWN;
			else if (hitbuttons & CBUT_ACTION)
				ret = MIU_TRIGGER;
			else if (hitbuttons & CBUT_RETURN)
				ret = MIU_CANCEL;
			else if (hitbuttons & CBUT_ACTION2)
				ret = MIU_TRIGGER2;
			else if (hitbuttons & CBUT_ACTION3)
				ret = MIU_TRIGGER3;
			else if (buttons & CBUT_DPAD_LEFT)
				ret = MIU_HOLDLEFT;
			else if (buttons & CBUT_DPAD_RIGHT)
				ret = MIU_HOLDRIGHT;
			else if (buttons & CBUT_DPAD_UP)
				ret = MIU_HOLDUP;
			else if (buttons & CBUT_DPAD_DOWN)
				ret = MIU_HOLDDOWN;
			else
				break;
			if (id) {
				switch (ret) {
					case MIU_HITLEFT:	ret = MIU_DEC;		break;
					case MIU_HITRIGHT:	ret = MIU_INC;		break;
					case MIU_HOLDLEFT:	ret = MIU_HOLDDEC;	break;
					case MIU_HOLDRIGHT:	ret = MIU_HOLDINC;	break;
//					case MIU_TRIGGER2:	ret = MIU_INC;		break;
				}
			}
			return ret;
		}
		case MMSG_CANCEL:
			return MIU_CANCEL;

		case MMSG_TRIGGER:
			if (id == "cancel")
				return MIU_CANCEL;
			return MIU_TRIGGER;

		case MMSG_TRIGGER2:
			if (id == "trigger2")
				return MIU_TRIGGER;
			return MIU_TRIGGER2;

		case MMSG_TRIGGER3:
			if (id == "trigger3")
				return MIU_TRIGGER;
			return MIU_TRIGGER3;

		case MMSG_GETFLOAT:
			if (!params) {
				static float t = 0;
				return (intptr_t)&t;
			}
			break;

		case MMSG_CUSTOMINIT:
			if (id == "selection")
				return 1;
			else if (id == "noselection")
				return -1;
			break;

		case MMSG_CUSTOMDRAW: {
			MenuCustomDraw *c		= static_cast<MenuCustomDraw*>(params);
			RenderRegion	*rr		= c->rr;
			GraphicsContext &ctx	= rr->ctx;
			ISO::Browser		args(c->GetArgs());

			if (id == "pointer") {
				if (pointer) {
					RenderRegion	*rr = c->rr;
					float2			tmouse	= project(concat(fmouse, one) / rr->GetMatrix3x3());
					save(rr->matrix, rr->matrix * translate(concat(tmouse - rr->size / 2, zero))), c->Draw(mi);
//					save(rr->matrix, rr->matrix * translate(float3(fmouse * rr->size / 2, zero))), c->Draw(mi);
//					save(rr->matrix,
//						float4x4(rr->matrix.x, rr->matrix.y, rr->matrix.z, float4(tmouse, zero, one))
//					), c->Draw(mi);
				} else {
					return -1;
				}
			} else if (id == "selection") {
				if (pointer) {
					if (c->rr->ContainsScreenPos(fmouse))
						return 2;
				}
			} else if (id == "custom_print") {
				rr->DrawText(mi->GetText(), args["flags"].GetInt(), 0.5f, args["shader"], c->GetArgs());
//				rr->DrawText(ISO_MenuGetText(), args["flags"].GetInt());
			} else if (id == "emitter") {
				// clip
				if (!(rr->cols[0].a > 0.0f && args.Count()))
					return -1;
				ISO::Browser _b;

				// spawn
				ParticleSet *ps = static_cast<ParticleSet*>(c->User());
				if (!ps && (_b = args[0]).Is("particle_effect")) {
					c->User() = ps = new ParticleSet(time);
					ps->Init(_b.GetName(), _b, identity);
					// rate
					if (!(_b = args["rate"]).Is<REGION_PARAM>())
						ps->ScaleEmitRate(_b.GetFloat(1.0f));
				}

				// update
				if (ps) {
					// rate
					if ((_b = args["rate"]).Is<REGION_PARAM>())
						ps->ScaleEmitRate(rr->GetField(*(REGION_PARAM*)_b));

					ps->SetMatrix(translate(rr->size.x * half + rr->offset.x, 0, -rr->size.y * half - rr->offset.y));
					// render
					float4x4	matrix		= rr->matrix;
					float		normalise	= rlen(matrix.x.xyz);
					matrix.x *= normalise;
					matrix.y *= normalise;
					matrix.z *= normalise;
					matrix.w *= normalise;

					ShaderConsts	rs(identity, identity, time);
					RenderEvent	re(ctx, rs);
					re.SetShaderParams();

					ps->Render(&re, (float3x4)float3x3(rotate_in_x(pi * half)), hardware_fix(matrix));
					ctx.SetUVMode(0, ALL_CLAMP);
					ctx.SetBackFaceCull(BFC_NONE);

					// update
					ps->Update(max(mi->GetTime() - time_last, zero));
				}
				return -1;

			} else if (id == "offscreen") {
				int		w = args["width"].GetInt(rr->size.x), h = args["height"].GetInt(rr->size.y);
				Texture	osb(TEXF_A8R8G8B8, w, h);
				{
					Surface	ss	= ctx.GetRenderTarget();
					RenderRegion	rr2 = *rr;
					rr2.matrix	= parallel_projection_rect(0.f, float(w), 0.f, float(h), 0.f, 1.f);
					rr2.size = float2{w, h};
					ctx.SetRenderTarget(osb.GetSurface());
					c->Draw(mi, &rr2);
					ctx.SetRenderTarget(ss);
				}
				static technique *tex_vc = ISO::root("data")["default"]["tex_vc"];
				rr->cols[0] = blend(rr->cols[0]);
				rr->Draw(osb, tex_vc, mi->GetBrowser(), args["align"].GetInt());
				return -1;

			} else if (id == "unwarp") {
				RenderRegion	rr2 = *rr;
				float2			size(rr2.size);

				position3	p0 = rr2.matrix * position3(zero, zero, one);
				position3	p1 = rr2.matrix * position3(size, one);

				float3x3	matC(
					float3{p1.v.x, p0.v.y, one},
					float3{p0.v.x, p1.v.y, one},
					concat(p0.v.xy * 2, 2)
				);
				float3x3	matP(
					float3{size.x,	zero,	one},
					float3{zero,	size.y,	one},
					float3{zero,	zero,	2}
				);

				float3x3	invP	= inverse(matP);
				float3x3	invC	= inverse(matC);
				float3		v		= (invC * float3{p1.v.x, p1.v.y, one}) / (invP * concat(size, one));

				matC.x *= v.x;
				matC.y *= v.y;
				matC.z *= v.z;

				float4x4	mat = float4x4(matC * invP);
				mat = transpose(mat); swap(mat.z, mat.w);
				mat = transpose(mat); swap(mat.z, mat.w);
				rr2.matrix	= mat;

				c->Draw(mi, &rr2);
				return -1;

			} else if (id == "truncate") {
				iso::Font	*font	= rr->fp->font;
				const char	*buffer = mi->GetText();
				const char	*linebreak;
				int		rw	= rr->size.x / rr->fp->scale;
				int		dw	= font->Width("...");
				int		tw	= font->Width(buffer, rw - dw, 1, &linebreak);
				if (linebreak)
					strcpy((char*)linebreak, "...");

			} else if (id == "colour_matrix") {
				AddShaderParameter("colour_matrix",		(void*)c->GetArgs()[0]);
				save(rr->technique, ISO::root("data")["menu"]["colouradj_texture"]), c->Draw(mi);
			}
			break;
		}

		case MMSG_CUSTOMUPDATE: {
			MenuCustomUpdate	*c	= static_cast<MenuCustomUpdate*>(params);
			ISO::Browser			args(c->GetArgs());

			if (id == "trigger") {
				return MIU_TRIGGER;

			} else if (id == "on_trigger") {
				int r = c->Update(mi);
				return r == MIU_TRIGGER ? args[0].GetInt() : r;

			}
			break;
		}

		case MMSG_EDITOR: {
			MenuEditor	*me		= (MenuEditor*)params;
			if (backup) {
				float3x3	mat3	= skipz(me->rr1->matrix);
				float2		mv2		= project(concat(fmouse_click, one) / mat3);
//				float2		mv2		= (position3(fmouse_click, zero) / me->rr1->matrix).xy;

				if (mv2.x > 0.f && mv2.x < me->rr1->size.x
				&&	mv2.y > 0.f && mv2.y < me->rr1->size.y)
				if (!--backup) {
					iso::iso_ptr32<const void>	t = me->p;
					currentsel		= (ISO_ptr<void>&)t;
					currentcxt		= me->context;
					currentspace	= me->rr0->matrix;
					currentsize		= me->rr0->size;
					postspace		= me->rr1->matrix;
					postsize		= me->rr1->size;
				}
			} else if (me->p == currentsel) {
				me->rr1->Fill(colour(1,1,1,0.5f));
				if (!currentcxt)
					currentcxt = me->context;
				if (me->context == currentcxt) {
					currentspace	= me->rr0->matrix;
					currentsize		= me->rr0->size;
					postspace		= me->rr1->matrix;
					postsize		= me->rr1->size;

					colour	col		= currentsel.IsType("mi_warp") ? colour(1,1,0) : currentsel.IsType("mi_offset") ? colour(0,0,1) : currentsel.IsType("mi_centre") ? colour(0,1,0) : colour(1,1,1);

					me->rr1->Line(col, 0,			0,			postsize.x,	0);
					me->rr1->Line(col, 0,			postsize.y,	postsize.x,	postsize.y);
					me->rr1->Line(col, 0,			0,			0,			postsize.y);
					me->rr1->Line(col, postsize.x,	0,			postsize.x,	postsize.y);
				}
			}
			break;
		}
	}
	return 0;
}

class EditorMenu : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type->Is("menu");
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr<void> &p) {
		ISO_ptr<void>	p2	= ISO_conversion::convert(p, NULL, ISO_conversion::RECURSE | ISO_conversion::CHECK_INSIDE | ISO_conversion::EXPAND_EXTERNALS);
		return *new ViewMenu(main, wpos, p2);
	}
} editormenu;
