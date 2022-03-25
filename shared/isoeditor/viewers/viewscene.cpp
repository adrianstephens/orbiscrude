#include "main.h"
#include "graphics.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "vector_string.h"
#include "thread.h"
#include "object.h"
#include "render.h"
#include "mesh/shapes.h"
#include "mesh/light.h"
#include "mesh/model_iso.h"
#include "render_object.h"
#include "ui/font.h"
#include "postprocess/post.h"

//template<> ISO::Type *ISO::getdef<ShaderConsts>();

using namespace app;

void DrawTextBox(GraphicsContext &ctx, const point &pos, const char *text) {
	static iso::Font *font	= ISO::root("data")["viewer_font"];
	static pass *font_dist	= *ISO::root("data")["default"]["font_dist_outline"][0];

	FontPrinter	fontprinter(ctx, font);
	auto	ext = fontprinter.CalcRect(text);

	static float outline		= 0.4f;
	float4		font_size		= concat(reciprocal(to<float>(font->Tex().Size())), float(font->outline), outline);
	float4x4	worldViewProj	= hardware_fix(parallel_projection_rect(float2(zero), to<float>(ctx.GetWindow().extent()), 0.f, 1.f));
	colour		outline_col(0,0,0,1);
	ShaderVal	vals[] = {
		{"worldViewProj",	&worldViewProj	},
		{"font_samp",		&font->Tex()	},
		{"font_size",		&font_size		},
		{"diffuse_colour",	&outline_col	}
	};

	ctx.SetBackFaceCull(BFC_NONE);
	ctx.SetDepthTestEnable(false);
	ctx.SetBlendEnable(true);
	ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);

#if 0
	Set(ctx, *root("data")["default"]["blend_vc"][0], ShaderVals(vals));
	PostEffects(ctx).DrawQuad(float2(pos.x, pos.y - font->baseline), float2(pos.x + ext.x, pos.y + ext.y - font->baseline), colour(1,1,1));

	Set(ctx, font_dist, ShaderVals(vals));
	fontprinter.SetColour(colour(0,0,0)).At(pos.x, pos.y).Print(text);
#else
	Set(ctx, font_dist, ISO::MakeBrowser(ShaderVals(vals)));
	fontprinter.SetColour(colour(1,1,1)).At(pos.x, pos.y).Print(text);
#endif
}

class ViewScene : public aligned<Window<ViewScene>, 128>, public WindowTimer<ViewScene>, com<IDropTarget>, Graphics::Display {
#ifdef ISO_EDITOR
	typedef IsoEditor	MainWindowType;
#else
	typedef MainWindow	MainWindowType;
#endif
	enum FLAGS {
		CALC_SCALE		= 1 << 0,
		FOCUS_POS		= 1 << 1,
		SHOW_GRID		= 1 << 2,
		SHOW_PLANE		= 1 << 3,
		SHOW_COMPASS	= 1 << 4,
		TARGETING		= 1 << 7,
	};
	MainWindowType			&main;
	flags<FLAGS>			flags;
	World					world;
	ISO_ptr<Scene>			scene;
	ShaderConsts			shader_consts;
	timer					time;
	rot_trans				view_loc;
	quaternion				lightdir	= identity;
	position3				focus_pos	= position3(zero);

	SurfaceT<float2>		cpu_depth;
	Fence					fence;
	float					move_scale	= 1;
	float					near_z;
	point					mouse_pos	= {-1, 0};
	RenderObject			*mouse_obj	= nullptr;
	Operation				*currentop	= nullptr;

	rot_trans				target_loc;
	GraphicsContext ctx;

	void		DrawScene(GraphicsContext &ctx);

	float4x4	ScreenProj() const {
		float2		size = to<float>(Size());
		return perspective_projection(1.f, size.x / size.y, near_z);
	}

	float4x4	ScreenMat() const {
		return scale(concat(to<float>(Size()) / 2, -0.5f)) * translate(1,1,-1) * ScreenProj() * (float3x4)view_loc;
	}

	SceneShaderConsts	CompassMats() const {
		return SceneShaderConsts((float3x4)float3x3(view_loc.rot), parallel_projection(0.7f, 0.7f, -10.f, 10.f));
	}

	int		GetCompassClick(point p) const {
		return p.x < 100 && p.y > Size().y - 100
			? GetAxesClick(CompassMats().viewProj0, (to<float>(p) - float2{50, Size().y - 50}) / 50.f)
			: 0;
	}

	float	GetMouseZ(const point &pt, int &i) const {
		ctx.Wait(fence);
		auto	over	= cpu_depth.Data()[pt.y][pt.x];
//		if ((i = over >> 24) && (over << 8))
//			return float(over << 8) / exp2(32.f);

		return over.x;

		plane	ground	= normalise(ScreenMat() * plane(z_axis, zero));
		return ground.dist(position3(to<float>(pt), 0));
	}

	position3	GetMouseScreen(const point &pt, int &i) const {
		return position3(to<float>(pt), GetMouseZ(pt, i));
	}

	//IDropTarget
	virtual HRESULT STDMETHODCALLTYPE DragEnter(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
		FORMATETC	fmte	= {CF_ISOPOD, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		STGMEDIUM	medium;
		HRESULT		hr;
		if (SUCCEEDED(hr = pDataObj->GetData(&fmte, &medium))|| true) {
			//ImageList	il	= ImageList::CreateLargeIcons(ILC_COLOR32, 1);
			//int	i = il.Add(Icon::Load(IDI_ISOPOD));
			//if (!il.DragBegin(i, Point(0, 0)))
			//	ISO_TRACEF("DragBegin fail:%i\n", GetLastError());

			ImageList::DragEnter(*this, Point(pt) - GetRect().TopLeft());
			*pdwEffect = DROPEFFECT_MOVE;
			return S_OK;
		}
		return hr;
	}
	virtual HRESULT STDMETHODCALLTYPE DragOver(DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
		ImageList::DragMove(Point(pt) - GetRect().TopLeft());
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE DragLeave() {
		ImageList::DragLeave(*this);
		//ImageList::DragEnd();
		return S_OK;
	}
	virtual HRESULT STDMETHODCALLTYPE Drop(IDataObject *pDataObj, DWORD grfKeyState, POINTL pt, DWORD *pdwEffect) {
		FORMATETC	fmte	= {CF_ISOPOD, NULL, DVASPECT_CONTENT, -1, TYMED_HGLOBAL};
		STGMEDIUM	medium;
		HRESULT		hr;
		if (SUCCEEDED(hr = pDataObj->GetData(&fmte, &medium))) {
			DragLeave();
			ISO_ptr<void>	&p	= *(ISO_ptr<void>*)*(ISO::Browser*)medium.hGlobal;
			scene->root->children.Append(p);

			if (CheckHasExternals(p, ISO::DUPF_DEEP)) {
				ISO::Type	*type = ISO::getdef<Model3>();
				p = (save(type->flags, type->flags & ~ISO::TypeUser::CHANGE), FileHandler::ExpandExternals(p));
			}

			p	= ISO_conversion::convert(p, p.GetType(), ISO_conversion::RECURSE | ISO_conversion::EXPAND_EXTERNALS | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK);
			float4x4	mat		= transpose(ScreenMat());
			mat.z	= z_axis;
			int			i;
			auto		temp = ToClient(Point(pt.x, pt.y));
			position3	pos	= GetMouseScreen(temp, i) / transpose(mat);
//			position3	pos	= project(GetMouseScreen(ToClient(Point(pt.x, pt.y)), i) / transpose(mat));

			make_GetSetter(&world, shader_consts), world.AddEntity(p, translate(pos));
			return S_OK;
		}
		return hr;
	}
public:

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam);
	ViewScene(MainWindow &_main, const WindowPos &wpos, const ISO_ptr_machine<void> p);
};

ViewScene::ViewScene(MainWindow &_main, const WindowPos &wpos, ISO_ptr_machine<void> p)
	: main((MainWindowType&)_main)
	, flags(SHOW_PLANE | SHOW_COMPASS | CALC_SCALE)
{
	Texture	*refmap = GetShaderParameter<crc32_const("_refmap")>();
	if (!*refmap)
		(ISO_ptr_machine<void>&)*refmap = ISO::root("data")["refmap"];

	{
		ISO::Type	*type = ISO::getdef<Model3>();
		save(type->flags, type->flags & ~ISO::TypeUser::CHANGE),
			scene = ISO_conversion::convert<Scene>(p, ISO_conversion::RECURSE | ISO_conversion::EXPAND_EXTERNALS | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK);
	}

	if (!scene->root)
		scene->root.Create();

	world.Use();
	world.Begin();
	make_GetSetter(&world, const_cast<const ShaderConsts&>(shader_consts)), world.AddEntity(scene->root);

	if (flags.test(CALC_SCALE))
		move_scale	= max(len(world.Extent().half_extent()), 1);
	near_z			= 0.1f;

	view_loc.rot	= rotate_in_x(pi * 0.6f);
	view_loc.trans4	= focus_pos - ~view_loc.rot * float3{0, move_scale, 0};
	target_loc		= view_loc;

	world.AddEntity("ambient", Light(ent::Light2::AMBIENT, colour(0.25f,0.25f,0.25f), 1000, 0));
	world.AddEntity("directional", Light(ent::Light2::DIRECTIONAL | ent::Light2::SHADOW, colour(1,1,1), 1000, 0));

	Create(wpos, get_id(p), CHILD | VISIBLE, CLIENTEDGE);
	RegisterDragDrop(*this, this);

	Timer::Start(0.01f);
}

LRESULT ViewScene::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_SIZE: {
			Point	size	= Point(lParam);
			SetSize((RenderWindow*)hWnd, size);
			if (size.x > 0 && size.y > 0) {
				cpu_depth	= SurfaceT<float2>(size, MEM_STAGING|MEM_CPU_READ);
			}
			break;
		}

		case WM_PAINT: {
			DeviceContextPaint	dc(*this);
			try {
				int			index;
				position3	w	= GetMouseScreen(mouse_pos, index) / ScreenMat();

				graphics.BeginScene(ctx);
				world.Use();
				DrawScene(ctx);

				if (!(GetMouseButtons() & (MK_LBUTTON | MK_RBUTTON)) && GetClientRect().Contains(ToClient(GetMousePos()))) {
					if (int c = flags.test(SHOW_COMPASS) ? GetCompassClick(mouse_pos) : 0) {
						if (all(GetAxesRot(c) == view_loc.rot))
							c = -c;
						DrawTextBox(ctx, mouse_pos, format_string("look along %c%c", c < 0 ? '-' : '+', "xyz"[abs(c) - 1]));
					} else {
						DrawTextBox(ctx, mouse_pos + point{32, 0}, buffer_accum<256>("over ") << index << '\n' << w);
					}
				}

				graphics.EndScene(ctx);
				Present();
			} catch (char *s) {
				MessageBoxA(*this, s, "Graphics Error", MB_ICONERROR | MB_OK);
			}
			break;
		}

		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			((IsoEditor&)main).SetEditWindow(*this);
			break;

		case WM_LBUTTONDOWN:
			((IsoEditor&)main).SetEditWindow(*this);

			if (int c = flags.test(SHOW_COMPASS) ? GetCompassClick(Point(lParam)) : 0) {
				quaternion	rot	= GetAxesRot(c);
				if (all(rot == target_loc.rot))
					rot = GetAxesRot(-c);

				target_loc = rotate_around_to(view_loc, focus_pos, rot);
				flags.set(TARGETING);

			} else {
				int		i	= 0;
				focus_pos	= GetMouseScreen(Point(lParam), i) / ScreenMat();
				while (any(focus_pos.v == infinity)) {
					_iso_break();
					focus_pos	= GetMouseScreen(Point(lParam), i) / ScreenMat();
				}
				mouse_obj	= i ? (RenderObject*)RenderEvent::items[i - 1].me : 0;
				move_scale	= len(view_loc * focus_pos);
				flags.set(FOCUS_POS);
			}
			currentop	= NULL;
			return 0;

		case WM_MOUSEMOVE: {
			point	new_mouse_pos	= Point(lParam);
			auto	dmouse			= mouse_pos - new_mouse_pos;

			if (wParam & MK_SHIFT) {
				// MOVE OBJECT
				if (mouse_obj) {
					int		i;
					float	mouse_z = GetMouseZ(mouse_pos, i);
					Object	*obj	= mouse_obj->obj;

					if (wParam & MK_RBUTTON) {
#ifdef ISO_EDITOR
						if (!currentop)
							main.Do(currentop = MakeModifyDataOp(ISO::Browser(obj->GetNode())));
#endif
						position3	p0	= position3(to<float>(mouse_pos), mouse_z) / ScreenMat();
						position3	p1	= position3(to<float>(new_mouse_pos), mouse_z) / ScreenMat();
						obj->SetMatrix(translate(p1 - p0) * obj->GetMatrix());

#ifdef ISO_EDITOR
						if (auto &node = obj->GetNode()) {
							node->matrix = obj->GetMatrix();
							main.Update(node);
						}
#endif

					} else if (wParam & MK_LBUTTON) {
#ifdef ISO_EDITOR
						if (!currentop)
							main.Do(currentop = MakeModifyDataOp(ISO::Browser(obj->GetNode())));
#endif
						position3	p0	= position3(to<float>(mouse_pos), mouse_z) / ScreenMat();
						position3	p1	= position3(to<float>(new_mouse_pos), mouse_z) / ScreenMat();
						position3	pc	= obj->GetPos();
						quaternion	q	= quaternion::between(p0 - pc, p1 - pc);

						obj->SetMatrix(obj->GetMatrix() * q);
#ifdef ISO_EDITOR
						if (auto &node = obj->GetNode()) {
							node->matrix = obj->GetMatrix();
							main.Update(node);
						}
#endif
					}
				}

			} else if (wParam & MK_CONTROL) {
				// MOVE LIGHT
				//quaternion	q(normalise(float4{float(mouse_pos.y - new_mouse_pos.y), float(new_mouse_pos.x - mouse_pos.x), 0, 512}));
				quaternion	q(normalise(to<float>(dmouse.yx, 0, 512)));
				lightdir		= normalise(q * lightdir);
				Light	light;
				light.col		= colour(1,1,1);
				light.range		= 1000;
				light.spread	= 0;
				light.matrix	= (float3x4)float3x3(lightdir);
				world.SetItem("directional", &light);
				//light_object.SetMatrix(float3x3(light));

			} else {
				// MOVE CAMERA
				if (wParam & MK_RBUTTON) {
					float3	t	= to<float>(dmouse, zero) * (move_scale / 256);
					view_loc *= translate(t);
					Invalidate();

				} else if (wParam & MK_LBUTTON) {
					float3	v		= view_loc.rot * float3(focus_pos);

					view_loc = view_loc * rotate_in_x(dmouse.y / 512.f) * rotate_in_z(dmouse.x / 512.f);

					//view_loc *= rotate_in_x(int(new_mouse_pos.y - mouse_pos.y) / 512.f);
					//view_loc *= rotate_in_y(int(new_mouse_pos.x - mouse_pos.x) / 512.f);

					if (flags.test(FOCUS_POS))
						view_loc *= translate(float3(v - view_loc.rot * float3(focus_pos)));
					Invalidate();
				}
			}

			mouse_pos	= new_mouse_pos;
			break;
		}

		case WM_MOUSEWHEEL: {
			float		dist	= (short)HIWORD(wParam) / 2048.f;
			position3	pos2	= view_loc * focus_pos;
			if (pos2.v.z > zero) {
				view_loc *= translate(pos2 * dist);
			} else {
				view_loc *= translate(float3{0, 0, move_scale * dist});
			}
			Invalidate();
			break;
		}

		case WM_ISO_TIMER: {
			if (flags.test(TARGETING)) {
				float		rate = 0.1f;
				view_loc = lerp(view_loc, target_loc, rate);
				if (approx_equal(view_loc, target_loc)) {
					view_loc = target_loc;
					flags.clear(TARGETING);
					//Stop();
				}
			}
			world.Tick1(time);
			world.Tick2();
			Invalidate();
			break;
		}

		case WM_COMMAND:
			if (LOWORD(wParam) == ID_EDIT_SELECT) {
				ISO::Browser b	= *(ISO::Browser*)lParam;
				const ISO::Type	*type = b.GetTypeDef();
				break;
			}
			break;

		case WM_NCDESTROY:
			RevokeDragDrop(*this);
			delete this;
			break;

		default:
			return Super(message, wParam, lParam);
	}
	return 0;
}

void ViewScene::DrawScene(GraphicsContext &ctx) {
	static pass *specular		= *ISO::root("data")["default"]["specular"][0];
	static pass *copy_depth		= *ISO::root("data")["default"]["copy_depth"][0];

	Rect		rect		= GetClientRect();
	Surface		screen		= GetDispSurface().As(TEXF_R8G8B8A8_SRGB);
	Surface		depth(TEXF_D24S8, screen.Size(), MEM_DEPTH);

	ctx.SetRenderTarget(screen);
	ctx.SetZBuffer(depth);

	colour	bgcol(0.5f,0.5f,0.5f);
	if (auto *bg = world.GetItem<Light>("background")) {
		bgcol = bg->col;
	}

	ctx.SetStencilFunc(STENCILFUNC_ALWAYS, 0);
	ctx.Clear(bgcol);
	ctx.SetBackFaceCull(BFC_BACK);

	ctx.SetDepthTestEnable(true);
	ctx.SetDepthTest(DT_USUAL);

	shader_consts	= ShaderConsts(view_loc, ScreenProj(), time);
	RenderEvent	render_event(ctx, shader_consts);

	render_event.Collect(&world);

	if (flags.test(CALC_SCALE)) {
		float	radius	= len(render_event.Extent().half_extent());
		if (radius > move_scale)
			move_scale = radius;
	}

	if (flags.test(SHOW_GRID)) {
		static pass *coloured			= *ISO::root("data")["default"]["coloured"][0];
		shader_consts.SetWorld(float3x4(scale(float3(move_scale))));
		Set(ctx, coloured, ISO::MakeBrowser(shader_consts));
		DrawWireFrameGrid(ctx, 20, 20);
	}

	render_event.window = iso::rect(rect.TopLeft(), rect.BottomRight());
	int	stencil_ref = 1;

	ctx.SetStencilOp(STENCILOP_KEEP, STENCILOP_KEEP, STENCILOP_REPLACE);

	for (auto &i : RenderEvent::items) {
		ctx.SetStencilRef(stencil_ref++);
		i(&render_event, i.extra);
	}

	ctx.SetStencilFunc(STENCILFUNC_OFF, 0);
	ctx.SetStencilOp(STENCILOP_KEEP, STENCILOP_KEEP, STENCILOP_KEEP);

	if (flags.test(SHOW_PLANE)) {
		ctx.SetBackFaceCull(BFC_NONE);
		ctx.SetBlendEnable(true);
		ctx.SetDepthTestEnable(true);
		colour		diffuse_colour(0, 0, 0, 0.5f);
		AddShaderParameter("diffuse_colour", diffuse_colour);
		shader_consts.tint = {0, 0, 0, 0.5f};

		static pass *grid_plane	= *ISO::root("data")["default"]["specular_grid"][0];
		shader_consts.SetWorld(identity);
		Set(ctx, grid_plane, ISO::MakeBrowser(shader_consts));
		DrawPlane(&render_event, z_axis);
	}

	*GetShaderParameter("shadowlight_dir") = shader_consts.iview * float3{0, 0, -1};
	*GetShaderParameter("shadowlight_col") = colour(1, 1, 1);

	if (mouse_obj) {// && (GetAsyncKeyState(VK_SHIFT) & 0x8000)) {
		ctx.SetDepthTestEnable(false);
//		ctx.SetDepthTest(DT_USUAL);
		ctx.SetBlendEnable(true);
		float3x4	world	= mouse_obj->obj->GetWorldMat();
		float		w		= (shader_consts.viewProj * get_trans(world)).v.w;
		DrawAxes(ctx, .05f, .2f, .2f, 0.25f, specular, world * scale(w / 16), ISO::MakeBrowser(shader_consts));
	}

	if (flags.test(SHOW_COMPASS)) {
		ctx.SetWindow(rect.Subbox(0, -100, 100, 0));
		ctx.SetBlendEnable(false);
		ctx.SetDepthTestEnable(true);
		ctx.SetDepthWriteEnable(true);

		(SceneShaderConsts&)shader_consts = CompassMats();
		DrawAxes(ctx, .05f, .2f, .2f, 0.25f, specular, identity, ISO::MakeBrowser(shader_consts));
	}

	ctx.SetWindow(rect);

	int					width = rect.Width(), height = rect.Height();
	TextureT<float2>	gpu_depth(width, height, 1, 1, MEM_WRITABLE);

	ctx.SetZBuffer({});
	ctx.SetShader(*copy_depth);
	ctx.SetTexture(SS_COMPUTE, depth.As(TEXF_R24X8), 0);
	ctx.SetTexture(SS_COMPUTE, depth.As(TEXF_X24G8), 1);
	ctx.SetRWTexture(SS_COMPUTE, gpu_depth);
	ctx.Dispatch(width / 16, height / 16);
	ctx.Blit(cpu_depth, gpu_depth);
	fence = ctx.PutFence();
	/*
	ctx.SetRenderTarget(gpu_depth);
	PostEffects(ctx).FullScreenQuad((*PostEffects::shaders->copy)[0]);
	ctx.Blit(cpu_depth, gpu_depth);
	*/
}

class EditorScene : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type == ISO::getdef<Scene>();
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr_machine<void> &p) {
		return *new ViewScene(main, wpos, p);
	}
	virtual ID GetIcon(const ISO_ptr<void> &p) {
		return "IDB_DEVICE_LANDSCAPE";
	}
} editorscene;
