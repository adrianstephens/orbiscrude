#include "main.h"
#include "graphics.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "vector_string.h"
#include "thread.h"

#include "3d/model_utils.h"
#include "mesh/model.h"
#include "mesh/light.h"
#include "mesh/patch.h"
#include "mesh/nurbs.h"
#include "mesh/shapes.h"
#include "animation/animation.h"
#include "animation/ik.h"
#include "postprocess/post.h"
#include "extra/octree.h"

#include "viewmesh.rc.h"

namespace ISO {
inline Browser MakeBrowser(arbitrary_ptr &t)	{
	return Browser(0, unconst((const void*)t));
}
}

using namespace app;

struct dual {
	float2	v;
	force_inline dual(float a, float b)		: v{a, b}	{}
	explicit force_inline dual(float2 a)	: v(a)		{}
};

dual operator*(param(dual) a, param(dual) b) {
	float2	t	= a.v * b.v.x;
	t.y += a.v.x * b.v.y;
	return dual(t);
}

dual inverse(param(dual) d) {
	float	r = reciprocal(d.v.x);
	return dual(r, -d.v.y * square(r));
}

dual sqrt(param(dual) d) {
	return dual(float2{one, d.v.y * half} * rsqrt(d.v.x));
}

force_inline dual norm(param(dual_quaternion) d)	{
	float	n = norm(d.r);
	return dual(n, cosang(d.r, d.t) / n);
}

typedef	pair<int,float>						MatrixPaletteWeight;
typedef	ISO_openarray<MatrixPaletteWeight>	MatrixPaletteEntry;
typedef	ISO_openarray<MatrixPaletteEntry>	MatrixPalette;

float4 rot_cones[] = {					// cones, dir.xyz,cos(angle)
	{ 0.0f,  0.0f,  1.0f, 0.95f},	// HIP_CENTER
	{ 1.0f,  0.0f,  0.0f, 0.64f},	// SPINE
	{ 1.0f,  0.7f,  0.0f, 0.76f},	// SHOULDER_CENTER (HEAD)
	{ 1.0f,  0.0f,  0.0f, 0.17f},	// SHOULDER_LEFT
	{ 1.0f,  0.0f,  0.0f, 0.17f},	// SHOULDER_RIGHT
	{ 0.0f,  0.0f, -1.0f, 0.00f},	// ELBOW_LEFT
	{ 0.0f,  0.0f,  1.0f, 0.00f},	// ELBOW_RIGHT
	{ 1.0f,  0.0f,  0.0f, 0.76f},	// WRIST_LEFT
	{ 1.0f,  0.0f,  0.0f, 0.76f},	// WRIST_RIGHT
	{ 1.0f,  0.0f,  0.0f, 0.34f},	// HIP_LEFT
	{ 1.0f,  0.0f,  0.0f, 0.34f},	// HIP_RIGHT
	{ 0.5f, -1.0f,  0.0f, 0.50f},	// KNEE_LEFT
	{ 0.5f, -1.0f,  0.0f, 0.50f},	// KNEE_RIGHT
	{ 1.0f,  1.0f,  0.0f, 0.50f},	// ANKLE_LEFT
	{ 1.0f,  1.0f,  0.0f, 0.50f},	// ANKLE_RIGHT
};

const char *rot_names[] = {
	"HIP_CENTER",
	"SPINE",
	"SHOULDER_CENTER",
	"SHOULDER_LEFT",
	"SHOULDER_RIGHT",
	"ELBOW_LEFT",
	"ELBOW_RIGHT",
	"WRIST_LEFT",
	"WRIST_RIGHT",
	"HIP_LEFT",
	"HIP_RIGHT",
	"KNEE_LEFT",
	"KNEE_RIGHT",
	"ANKLE_LEFT",
	"ANKLE_RIGHT",
};

#if 0
ISO_ptr<SubMesh> GenerateHull(ISO_ptr<SubMesh> submesh) {
	uint32	vertsize	= submesh->VertSize();
	int		nverts		= submesh->NumVerts();
	char	*verts		= submesh->Verts();

	position3			*pos		= new aligned<position3,16>[nverts];
	uint16				*indices	= new uint16[nverts];
	for (int i = 0; i < nverts; i++)
		pos[i] = float3((const float*)(verts + i * vertsize));

	int	ntris = generate_hull_3d(pos, nverts, indices);

	ISO_ptr<SubMesh> hull = Duplicate("hull", submesh);
	memcpy(hull->indices.Resize(ntris), indices, ntris * 6);
	return hull;
}
#endif

bool FindIndex(const ISO::Browser &bi, const ISO::Browser &b, int &index) {
	const ISO::Type	*type = b.GetTypeDef();
	if (bi[0].Is(type) && (char*)b >= bi[0] && (char*)b <= bi[bi.Count() - 1]) {
		index	= ((char*)b - bi[0]) / bi[0].GetSize();
		return true;
	}
	return false;
}

bool FindIndex(Model3 *model, const ISO::Browser &b, int &sub, int &index) {
	for (int i = 0, n = model->submeshes.Count(); i < n; i++) {
		if (FindIndex(ISO::Browser(model->submeshes[i])["indices"], b, index)) {
			sub		= i;
			return true;
		}
	}
	return false;
}

bool FindSubMesh(Model3 *model, void *v, int &sub) {
	for (int i = 0, n = model->submeshes.Count(); i < n; i++) {
		if (&*model->submeshes[i] == v) {
			sub = i;
			return true;
		}
	}
	return false;
}

bool FindVertex(Model3 *model, void *v, int &sub, int &index) {
	for (int i = 0, n = model->submeshes.Count(); i < n; i++) {
		SubMesh				&submesh	= (SubMesh&)*model->submeshes[i];
		ISO_openarray<char>	*verts		= submesh.verts;
		char				*vertsp		= *verts;

		if ((char*)v >= vertsp) {
			uint32	subsize = ((ISO::TypeOpenArray*)(submesh.verts).GetType())->subsize;
			int		r		= ((char*)v - vertsp) / subsize;
			if (r >= 0 && r < verts->Count()) {
				sub		= i;
				index	= r;
				return true;
			}
		}
	}
	return false;
}

bool FindVertex(PatchModel3 *patch, void *v, int &sub, int &pat, int &index) {
	for (int i = 0, n = patch->subpatches.Count(); i < n; i++) {
		SubPatch&			subpatch	= patch->subpatches[i];
		ISO_openarray<char>	*verts		= subpatch.verts;
		char				*vertsp		= *verts;

		if ((char*)v >= vertsp) {
			uint32	subsize = ((ISO::TypeOpenArray*)(subpatch.verts).GetType())->subsize;
			int		r		= ((char*)v - vertsp) / subsize;
			if (r >= 0 && r < verts->Count()) {
				sub		= i;
				pat		= r;
				index	= (((char*)v - vertsp) % subsize) / sizeof(float[3]);
				return true;
			}
		}
	}
	return false;
}

bool HasIntVerts(SubMesh *sm) {
	return sm->VertComponent(0)->type->SubType()->GetType() == ISO::INT;
}

/*
rot_trans lerp(rot_trans &a, const rot_trans &b, float rate) {
	quaternion	dq	= b.rot / a.rot;
	float3x3	m	= look_along_z(dq.xyz);

	float3x4	t0	= a;
	float3x4	t1	= b;

	float3x4	dm	= t1 - t0;
	float3x4	dm2	= transpose(m) * dm * m;
//	float3x4	dm2	= dm * m;
	float2x3	dm3	= strip_z(dm2);

	auto		rot		= normalise(slerp(a.rot, b.rot, rate));
	float4		trans4;
	if (dm3.det() > 0.001f) {
		float3x4	tm	= transpose(m) * t0 * m;
		//float3x4	tm	= t0 * m;

		float3	c	= float3(position2(zero) / dm3, -tm.w.z);
		c = m * c;
		float3	v	= a.rot * float3(c);
		trans4	= a.trans4.xyz + v - a.rot * c;
	} else {
		trans4	= lerp(a.trans4, b.trans4, rate);
	}

	return rot_trans(rot,  trans4);
}
*/
rot_trans as_rot_trans(const triangle3 &t) {
	return rot_trans(quaternion::between(z_axis, -t.normal()), concat(t.centre(), t.area()));
}

class ViewModel : public Window<ViewModel>, public refs<ViewModel>, public WindowTimer<ViewModel>, public World, Graphics::Display {

	enum FLAGS	{
		MODE			= 3 << 0,
			POINTS			= 0 << 0,
			WIREFRAME		= 1 << 0,
			FILL			= 2 << 0,
		SCISSOR			= 1 << 2,
		FRUSTUM_PLANES	= 1 << 3,
		FRUSTUM_EDGES	= 1 << 4,
		BOUNDING_EDGES	= 1 << 5,
		SCREEN_RECT		= 1 << 6,
		BONES			= 1 << 7,
		SLOW			= 1 << 8,
		MOVING			= 1 << 9,
		CUSTOM1			= 1 << 16,
		CUSTOM2			= 1 << 17,
	};

	struct Selection {
		enum MODE {
			NONE, VERTEX, FACE, PATCH, SUBMESH, BONE
		};
		MODE	mode;
		int		index;
		Selection(MODE mode = NONE, int index = -1) : mode(mode), index(index) {}
		explicit constexpr operator bool()		const { return mode != NONE; }
		bool	operator==(const Selection &b)	const { return mode == b.mode && index == b.index; }
		bool	operator!=(const Selection &b)	const { return mode != b.mode || index != b.index; }
	};

	MainWindow				&main;
	Accelerator				accel;
	BackFaceCull			cull;
	iso::flags<FLAGS>		flags;
	ToolBarControl			toolbar;
	ToolTipControl			tooltip;
	timer					time;
	ISO_ptr_machine<void>	original;
	ISO_ptr<Node>			node;
	ISO_ptr<Model3>			model;
	ISO_ptr<PatchModel3>	patch;
	ISO_ptr<NurbsModel>		nurbs;
	ISO_ptr<BasePose>		basepose;

	TesselationInfo			tess_info;
	Tesselation				tess;
	octree					oct;
	dynamic_array<float3p>	verts;

	Selection				sel;
	Selection				hover;
	int						sel_patch;
	rot_trans				sel_loc;
	float3x4p*				sel_mat	= nullptr;

	TextureT<float4>		bone_texture;

	float					move_scale;
	float					near_z;
	Point					prevmouse;
	rot_trans				view_loc;
	rot_trans				target_loc;
	position3				focus_pos;
	quaternion				light;
	colour					light_ambient;
	AnimationHolder			*anim;

	JointRotFilter::entry	filter_entries[IK_MAX_BONES];
	JointRotFilter			filter;

	ISO_ptr<SubMeshBase>	GetSelectedSubMesh(const Selection &sel, int &sub_sel)	const;
	position3				GetVertex(int i)		const;
	triangle3				GetTriangle(int i)		const;
	
	Selection		TestMouse(const point &mouse);
	Selection		ToSelection(ISO::Browser b);
	bool			SetSelection(const Selection &sel0);
	bool			SetAnimation(ISO_ptr<Animation> t);
	void			GetSelectionText(string_accum &sa, const Selection &sel);

	void			DrawSelection(GraphicsContext &ctx, const Selection &sel, const ISO::Browser &parameters);
	void			DrawScene(GraphicsContext &ctx);
	void			DrawMatrix(GraphicsContext &ctx, param(float3x4) mat, pass *p, ISO::Browser params);
	void			ResetView();
	bool			ClickAxes(const point &mouse);

	float4x4		Projection() const {
		return perspective_projection(1.f, float(width) / height, near_z);
	}

	float4x4		ScreenMat() const {
		float3x4	view	= view_loc;
		return scale(concat(to<float>(Size()) / 2, 1)) * translate(1,1,0) * Projection() * view;
	}

	void			ChangeView(const rot_trans &rt) {
		target_loc	= rt;
		flags.set(MOVING);
		Timer::Start(0.01f);
	}

	void			StopMoving() {
		flags.clear(MOVING);
		Timer::Stop();
	}

public:

	struct vertex {
		float3p		pos;
		rgba8		col;
		void		set(param(float3) p, param(colour) c) { pos = p; col = c.rgba; }
	};

	LRESULT Proc(MSG_ID message, WPARAM wParam, LPARAM lParam);

	ViewModel(MainWindow &_main, const WindowPos &wpos, const ISO_ptr_machine<void> p);
};

LRESULT ViewModel::Proc(MSG_ID message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_CREATE: {
			toolbar.Create(*this, NULL, CHILD | CLIPSIBLINGS | VISIBLE | toolbar.NORESIZE | toolbar.FLAT | toolbar.TOOLTIPS);
			toolbar.Init("IDR_TOOLBAR_MESH");
			//toolbar.Add(Menu("IDR_MENU_VIEWMODEL"));
			toolbar.CheckButton(ID_MESH_BACKFACE, cull == BFC_FRONT);
			toolbar.CheckButton(ID_MESH_FILL, flags.test(FILL));

			tooltip.Create(*this, NULL, POPUP);// | TTS_NOPREFIX | TTS_ALWAYSTIP);
			tooltip.Add(*this);

			Accelerator::Builder	ab;
			ab.Append(42, ' ');
			accel	= Accelerator(ab);

			return 0;
		}

		case WM_SIZE:
			SetSize((RenderWindow*)hWnd, Point(lParam));
			toolbar.Resize(toolbar.GetItemRect(toolbar.Count() - 1).BottomRight() + Point(3, 3));
			break;

		case WM_PAINT: {
			DeviceContextPaint	dc(*this);
			try {
				GraphicsContext ctx;
				graphics.BeginScene(ctx);
			#if 1
				Surface	screen = GetDispSurface();// .As(TEXF_R8G8B8A8_SRGB);
			#else
				Texture	screen(TEXF_R8G8B8A8,	Size().x, Size().y, 1, 1, MEM_TARGET);
			#endif
				ctx.SetRenderTarget(screen);
				ctx.SetZBuffer(Surface(TEXF_D24S8, Size(), MEM_DEPTH));
				ctx.Clear(colour(0.5f,0.5f,0.5f));
				ctx.SetBackFaceCull(BFC_BACK);
				ctx.SetDepthTestEnable(true);
				ctx.SetDepthTest(DT_USUAL);
			#ifndef USE_DX11
				ctx.SetTexture(bone_texture, D3DVERTEXTEXTURESAMPLER0);
			#endif
				Use();
				DrawScene(ctx);

				//ctx.SetDepthTestEnable(false);
				//ctx.SetRenderTarget(Surface(), RT_DEPTH);
				//ctx.SetRenderTarget(GetDispSurface());
				//PostEffects(ctx).FullScreenQuad((*PostEffects::shaders->copy)[0]);

				graphics.EndScene(ctx);
				Present();
			} catch (char *s) {
				MessageBoxA(*this, s, "Graphics Error", MB_ICONERROR | MB_OK);
			}
			break;
		}

		case WM_MOUSEACTIVATE:
			SetFocus();
			return MA_ACTIVATE;

		case WM_SETFOCUS:
			SetAccelerator(*this, accel);
			break;

		case WM_RBUTTONDOWN:
		case WM_MBUTTONDOWN:
			prevmouse	= Point(lParam);
			((IsoEditor&)main).SetEditWindow(*this);
			break;

		case WM_LBUTTONDOWN:
			prevmouse	= Point(lParam);
			((IsoEditor&)main).SetEditWindow(*this);
			if (!(wParam & (MK_SHIFT | MK_CONTROL)) && !(GetKeyState(VK_MENU) & 0x8000)) {
				if (ClickAxes(prevmouse) || SetSelection(TestMouse((prevmouse))))
					Invalidate();
			}
			break;

		case WM_LBUTTONDBLCLK:
			ResetView();
			break;

		case WM_MOUSEMOVE: {
			Point		mouse(lParam);
			if (wParam & MK_MBUTTON) {
				if (sel.mode == Selection::BONE) {
					Pose		*pose	= Root()->Property<Pose>();
					float3x4	wmat	= pose->GetObjectMatrix(sel.index);
					float3		axis	= wmat * float3{float(prevmouse.y - mouse.y), 0, float(prevmouse.x - mouse.x)};
					quaternion	q(normalise(concat(axis, 512)));
					pose->joints[sel.index].rot *= q;
					pose->mats[sel.index] = pose->joints[sel.index];
					pose->Update();
					Invalidate();
				}
			} else if (wParam & MK_RBUTTON) {
				StopMoving();
				view_loc *= translate(float3{float(mouse.x - prevmouse.x), float(mouse.y - prevmouse.y), 0} * (move_scale / 256));
				Invalidate();
				Update();

			} else if (wParam & MK_LBUTTON) {
				if (sel.mode == Selection::BONE) {
					Pose *pose = Root()->Property<Pose>();
					pose->mats[sel.index] = pose->mats[sel.index] * rotate_in_y((mouse.x - prevmouse.x) / 100.f) * rotate_in_x((mouse.y - prevmouse.y) / 100.f);

				} else {
					quaternion	q(normalise(float4{float(prevmouse.y - mouse.y), float(mouse.x - prevmouse.x), 0, 512}));
					if (wParam & MK_CONTROL) {
						light	= normalise(q * light);
					} else {
						StopMoving();
						view_loc = rotate_around(view_loc, GetKeyState(VK_MENU) & 0x8000 ? position3(sel_loc.trans4.xyz) : focus_pos, q);
					}
				}
				Invalidate();
				Update();
			} else {
				auto	hover0 = TestMouse(mouse);
				if (hover != hover0) {
					hover = hover0;
					tooltip.Activate(*this, hover.mode != Selection::NONE);
					Invalidate();
					Update();
				}
			}
			prevmouse	= mouse;
			tooltip.Track();
			break;
		}

		case WM_MOUSELEAVE:
			tooltip.Activate(*this, *this, false);
			return 0;

		case WM_MOUSEWHEEL: {
			StopMoving();
			float		dist	= (short)HIWORD(wParam) / 2048.f;
			position3	pos		= GetAsyncKeyState(VK_MENU) & 0x8000 ? position3(sel_loc.trans4.xyz) : focus_pos;
			position3	pos2	= view_loc * pos;
			if (pos2.v.z > zero) {
				view_loc *= translate(pos2 * dist);
				near_z = min(move_scale / 100, pos2.v.z / 2.f);
			} else {
				view_loc *= translate(float3{0, 0, move_scale * dist});
			}
			Invalidate();
			Update();
			break;
		}

		case WM_ISO_TIMER: {
			bool	stop = true;
			if (anim) {
				float	t = time;
				if (flags.test(SLOW))
					t /= 10;
				stop = !anim->Evaluate(Root()->Property<Pose>(), t);
			}
			if (flags.test(MOVING)) {
				float		rate	= 0.1f;
				view_loc = lerp(view_loc, target_loc, rate);
				if (approx_equal(view_loc, target_loc, 1e-4f)) {
					view_loc = target_loc;
				} else {
					stop = false;
				}
				if (sel.mode == Selection::FACE) {
					position3	pos	= view_loc * position3(sel_loc.trans4.xyz);
					if (pos.v.z > zero)
						near_z = min(move_scale / 100, pos.v.z / 2.f);
				}

			}
			Invalidate();
			if (stop)
				StopMoving();
			break;
		}

		case WM_COMMAND:
			switch (uint16 id = LOWORD(wParam)) {
				case ID_EDIT_SELECT: {
					flags.clear(BONES);
					auto	b = *(ISO::Browser*)lParam;
					const ISO::Type	*type = b.GetTypeDef();
					if (SkipUserType(type) == ISO::REFERENCE) {
						b		= *b;
						type	= b.GetTypeDef();
						if (type->Is("Animation")) {
							return SetAnimation(b);

						} else if (type->Is("AnimationHierarchy")) {
							Root()->AddEntities(b);
							return true;
						}
					} else if (type->SameAs<float3x4p>()) {
						sel_mat		= (float3x4p*)b;
						Invalidate();
						return true;
					}

					if (SetSelection(ToSelection(b))) {
						Invalidate();
						return true;
					}
					return false;
				}

				case ID_MESH_BACKFACE:
					cull = ~cull;
					toolbar.CheckButton(id, cull == BFC_FRONT);
					Invalidate();
					break;

				case ID_MESH_RESET:
					ResetView();
					break;

				case ID_MESH_BOUNDS:
					toolbar.CheckButton(id, flags.flip_test(BOUNDING_EDGES));
					Invalidate();
					break;

				case ID_MESH_FILL:
					flags.inc_field(MODE);
					//toolbar.CheckButton(id, flags.flip_test(FILL));
					Invalidate();
					break;

				case ID_VIEWMODEL_SLOW:
					flags.flip(SLOW);
					break;

				case 42:
					flags.flip(CUSTOM1);
					Invalidate();
					break;
			}
			break;

		case WM_NOTIFY: {
			NMHDR	*nmh = (NMHDR*)lParam;
			switch (nmh->code) {
				case TBN_DROPDOWN: {
					NMTOOLBAR	*nmtb	= (NMTOOLBAR*)nmh;
					Menu		menu	= ToolBarControl(nmh->hwndFrom).GetItem(nmtb->iItem).Param();
					menu.CheckByID(ID_VIEWMODEL_SLOW, flags.test(SLOW));
					menu.Track(*this, ToScreen(Rect(nmtb->rcButton).BottomLeft()));
					break;
				}
				case TTN_GETDISPINFOA:
					if (nmh->hwndFrom == tooltip) {
						NMTTDISPINFOA	*nmtdi	= (NMTTDISPINFOA*)nmh;
						GetSelectionText(lvalue(fixed_accum(nmtdi->szText)), hover);
					}
					break;
			}
			break;
		}
		case WM_NCDESTROY:
			tooltip.Destroy();
			release();
			break;
		default:
			return Super(message, wParam, lParam);
	}
	return 0;
}
ViewModel::Selection ViewModel::ToSelection(ISO::Browser b) {
	if (const ISO::Type	*type = b.GetTypeDef()) {
		if (SkipUserType(type) == ISO::REFERENCE) {
			b		= *b;
			type	= b.GetTypeDef();
		}
		if (type->SameAs<SubMeshBase>()) {
			int	submesh;
			if (model && (FindSubMesh(model, b, submesh) || (original.IsType<Model3>() && FindSubMesh((Model3*)original, b, submesh))))
				return {Selection::SUBMESH, submesh};

		} else if (type->SameAs<Bone>()) {
			if (BasePose *bp = basepose) {
				Bone	*bone = b;
				for (int i = 0, n = bp->Count(); i < n; i++) {
					if (bone == (*bp)[i])
						return {Selection::BONE, i};
				}
			}

		} else if (model && type->GetType() == ISO::ARRAY && type->SubType()->SameAs<uint16>()) {
			int	submesh, index;
			if (FindIndex(model, b, submesh, index) || (original.IsType<Model3>() && FindIndex((Model3*)original, b, submesh, index)))
				return {Selection::FACE, index};

			if (ISO::Browser bi	= ISO::Browser(original)["indices"]) {
				if (FindIndex(bi, b, index))
					return {Selection::FACE, index};
			}

		} else if (model) {
			int	submesh, index;
			if (FindVertex(model, b, submesh, index) || (original.IsType<Model3>() && FindVertex((Model3*)original, b, submesh, index)))
				return  {Selection::VERTEX, index};

		} else if (patch) {
			int	submesh, index;
			if (FindVertex(patch, b, submesh, sel_patch, index) || (original.IsType<PatchModel3>() && FindVertex((PatchModel3*)original, b, submesh, sel_patch, index)))
				return {index < 16 ? Selection::VERTEX : Selection::PATCH, index};
		}
	}
	return {};
}

ISO_ptr<SubMeshBase> ViewModel::GetSelectedSubMesh(const Selection &sel, int &sub_sel) const {
	int		i = sel.index;
	if (sel.mode == Selection::SUBMESH)
		return model->submeshes[i];

	for (auto &sm : model->submeshes) {
		auto	n = sel.mode == Selection::VERTEX ? ((SubMesh*)sm)->NumVerts() : ((SubMesh*)sm)->NumFaces();
		if (i < n) {
			sub_sel = i;
			return sm;
		}
		i -= n;
	}
	return ISO_NULL;
}

position3 ViewModel::GetVertex(int i) const {
	if (verts)
		return position3(verts[i]);

	int		sub_sel;
	SubMesh *sm = GetSelectedSubMesh({Selection::VERTEX, i}, sub_sel);
	return position3(sm->VertComponentData<float3p>(0)[sub_sel]);
}

triangle3 ViewModel::GetTriangle(int i) const {
	auto	_GetTriangle = [](auto v, uint32 f[3]) { return triangle3(position3(v[f[0]]), position3(v[f[1]]), position3(v[f[2]])); };

	const float3p	*v = verts.begin();
	for (SubMesh *sm : model->submeshes) {
		auto	n = sm->NumFaces();
		if (i < n) {
			if (v)
				return _GetTriangle(v, sm->indices[i]);
			else
				return _GetTriangle(sm->VertComponentData<float3p>(0), sm->indices[i]);
		}
		if (v)
			v += sm->NumVerts();
		i -= n;
	}
	unreachable();
}


void ViewModel::ResetView() {
	ChangeView(translate(zero, zero, sqrt(sel_loc.trans4.w) * two) / sel_loc);
	//ChangeView(rot_trans(rotate_in_x(pi * half), position3(zero, zero, move_scale / 2)));
}

bool ViewModel::ClickAxes(const point &mouse) {
	if (int axis = GetAxesClick(parallel_projection(0.7f, 0.7f, -10.f, 10.f) * (float3x3)view_loc.rot, float2(mouse - point{50, height - 50}) / float2(50))) {
		quaternion	rot	= GetAxesRot(axis);
		if (all(rot == target_loc.rot))
			rot = rotate_in_x(pi) * rot;

//		float3		c2	= view_loc.rot * float3(focus_pos);
//		ChangeView(rot_trans(q, view_loc.trans + q * -focus_pos + c2));
		ChangeView(rotate_around_to(view_loc, focus_pos, rot));
		return true;
	}
	return false;
}

ViewModel::Selection ViewModel::TestMouse(const point &mouse) {
	if (oct.root) {
		int			total	= 0;
		float4x4	mat		= translate(-mouse.x, -mouse.y, 0) * ScreenMat();

		int			face	= oct.shoot_ray(mat, 0.25f, [this](int i, param(ray3) r, float &t) {
			triangle3	tri	= GetTriangle(i);
			float3		normal;
			return tri.ray_check(r, t, &normal) && dot(normal, r.d) < zero;
		});

		if (face >= 0) {
			int		face_offset = face;
			int		vert_offset	= 0;
			SubMesh::face	indices;
			for (SubMesh *sm : model->submeshes) {
				auto	n = sm->NumFaces();
				if (face_offset < n) {
					indices = sm->indices[face_offset];
					break;
				}
				vert_offset += sm->NumVerts();
				face_offset -= n;
			}

			auto	tri		= GetTriangle(face);
			float	mind	= 10;
			int		index;

			for (int i = 0; i < 3; i++) {
				position3	p	= tri.corner(i);
				float		d	= len2(project(mat * float4(p)).v.xy);
				if (d < mind) {
					mind	= d;
					index	= i;
				}
			}
			if (mind == 10)
				return {Selection::FACE, face};
			else
				return {Selection::VERTEX, indices[index] + vert_offset};
		}
	}
	return {Selection::NONE};
}

bool ViewModel::SetSelection(const Selection &sel0) {
	if (sel == sel0)
		return false;

	if (!sel0.mode)
		return false;

	sel		= sel0;
	switch (sel0.mode) {
		case Selection::VERTEX: {
			int			sub_sel;
			SubMesh		*sm = GetSelectedSubMesh(sel, sub_sel);
			sel_loc		= rot_trans(identity, position3(sm->VertComponentData<float3p>(0)[sub_sel]));
			return true;
		}
		case Selection::FACE:
			sel_loc	= as_rot_trans(GetTriangle(sel0.index));	//	centre / 3;
			return true;

		case Selection::BONE:
			sel_mat		= nullptr;
			sel_loc		= rot_trans(identity, focus_pos);
			flags.set(BONES);
			return true;

		default:
			sel_loc		= rot_trans(identity, focus_pos);
			return true;
	}
}

void ViewModel::GetSelectionText(string_accum& sa, const Selection& sel) {
	switch (sel.mode) {
		case Selection::VERTEX: {
			int			sub_sel;
			SubMesh		*sm		= GetSelectedSubMesh(sel, sub_sel);
			auto		&vert	= sm->VertComponentData<float3p>(0)[sub_sel];
			sa << "vertex " << sub_sel << '/' << sm->NumVerts() << ":(" << vert.x << ',' << vert.y << ',' << vert.z << ')';
			break;
		}
		case Selection::FACE: {
			int			sub_sel;
			SubMesh		*sm		= GetSelectedSubMesh(sel, sub_sel);
			auto		&face	= sm->indices[sub_sel];
			sa << "face " << sub_sel << '/' << sm->NumFaces() << ":(" << face[0] << ',' <<face[1] << ',' << face[2] << ')';;
			break;
		}

		case Selection::SUBMESH:
			sa << "submesh";
			break;

		case Selection::BONE:
			sa << "bone";
			break;

		default:
			break;
	}
}


bool ViewModel::SetAnimation(ISO_ptr<Animation> t) {
	if (anim)
		delete anim;
	anim = new AnimationHolder(Root(), t);
	Timer::Start(1/60.f);
	time.reset();
	return true;
}

void ViewModel::DrawSelection(GraphicsContext &ctx, const Selection &sel, const ISO::Browser &parameters) {
	int						sub_sel;
	ISO_ptr<SubMeshBase>	smb		= GetSelectedSubMesh(sel, sub_sel);

	if (((pass*)(*smb->technique)[0])->Has(SS_VERTEX)) {
		SubMeshPlat::renderdata	*rd		= (SubMeshPlat::renderdata*)((SubMesh*)smb)->verts.User().get();
		bool					pass	= HasIntVerts(smb);
		ctx.SetVertexType(rd->vd);
		ctx.SetVertices(0, rd->vb, rd->vert_size);
		ctx.SetIndices(rd->ib);

		switch (sel.mode) {
			case Selection::VERTEX: {
			#ifndef USE_DX11
				ctx.Device()->SetRenderState(D3DRS_POINTSIZE, iorf(8.f).i);
			#else
				Set(ctx, *ISO::root("data")["default"]["thickpoint"][pass], parameters);
			#endif
				ctx.DrawPrimitive(PRIM_POINTLIST, sub_sel, 1);
				break;
			}
			case Selection::FACE: {
				uint8	vpp		= smb->GetVertsPerPrim();
				Set(ctx, *ISO::root("data")["default"]["coloured"][pass], parameters);
				ctx.DrawIndexedVertices(PrimType(rd->prim), 0, rd->nverts, sub_sel * vpp, vpp);
				break;
			}
			case Selection::SUBMESH: {
				Set(ctx, *ISO::root("data")["default"]["coloured"][pass], parameters);
				ctx.DrawIndexedVertices(PrimType(rd->prim), 0, rd->nverts, 0, rd->nindices);
				break;
			}
		}
	}
}

void ViewModel::DrawScene(GraphicsContext &ctx) {
	static pass *specular		= *ISO::root("data")["default"]["specular"][0];

	float4x4	proj0			= Projection();

	float3x4	view			= view_loc;
	float3x4	iview			= inverse(view);
	float4x4	proj			= hardware_fix(proj0);
	float4x4	viewProj		= proj * view;

	float3x4	world			= Root()->GetWorldMat();
	float3x4	worldView		= view * world;
	float4x4	worldViewProj	= viewProj * world;

	float3		shadowlight_dir	= -float3x3(light).z;
	colour		shadowlight_col(one, one ,one);
	colour		tint(1,1,1);

	float2		point_size		= float2{8.f / width, 8.f / height};

	SH			diffuse_irradiance;
	int			num_lights		= 0;
	float4		fog_dir			= float4(zero);
	colour		fog_col			= colour(zero);
	float		glossiness		= 8;

	diffuse_irradiance.AddAmbient(light_ambient);

#if 0
	static DataBufferT<int,false>			visible_meshlets(0x100000, MEM_WRITABLE);//|MEM_CPU_READ);
	static DataBufferT<array<int,3>, false>	meshlet_indices;

	if (!meshlet_indices) {
		SubMesh	*sm = model->submeshes[0];
		pass	*p = (*sm->technique)[0];

		if (!p->Has(SS_VERTEX)) {
			auto	prims = ISO::Browser(sm->parameters)["prims"];
			uint32*	prims_start		= prims[0];
			uint32	prims_num		= prims.Count();
			dynamic_array<array<int,3>>	tri_indices;
			uint32	*indices		= sm->indices.begin()->begin();

			struct Meshlet {
				uint32	PrimCount, PrimOffset;
				float4p	sphere;
				xint32	cone;
				float	apex;
			};

			for (Meshlet *m : ISO::Browser(sm->parameters)["meshlets"]) {
				auto	*prims = prims_start + m->PrimOffset;
				for (int i = 0; i < m->PrimCount; i++) {
					uint32	prim = *prims++;
					auto&	dest	= tri_indices.push_back();
					dest[0] = indices[(prim & 0xff)];
					dest[1] = indices[((prim >>  8) & 0xff)];
					dest[2] = indices[((prim >> 16) & 0xff)];
				}
				indices += 64;
			}

			meshlet_indices.Init(tri_indices.begin(), tri_indices.size32(), MEM_WRITABLE);//|MEM_CPU_READ);
		} else {
			meshlet_indices.Init(0x100000, MEM_WRITABLE);//|MEM_CPU_READ);
		}
	}
#elif 0
	static DataBufferT<int,false>			visible_meshlets;
	static DataBufferT<array<int,3>, false>	meshlet_indices(0x100000, MEM_WRITABLE);//|MEM_CPU_READ);
	DataBufferT<DrawVerticesArgs>		indirect2({{0, -1, 0, 1}}, MEM_WRITABLE|MEM_INDIRECTARG);

	if (!visible_meshlets) {
		SubMesh	*sm = model->submeshes[0];
		pass	*p = (*sm->technique)[0];

		if (!p->Has(SS_VERTEX)) {
			dynamic_array<int>	meshlets = int_range(sm->NumFaces());
			visible_meshlets.Init(meshlets.begin(), meshlets.size(), MEM_WRITABLE);//|MEM_CPU_READ);
		}
	}

#else
	DataBufferT<int,false>		visible_meshlets(0x100000, MEM_WRITABLE|MEM_CPU_READ);
	DataBufferT<int[4], false>	meshlet_indices(0x1000000, MEM_WRITABLE);//|MEM_CPU_READ);
	static DataBufferT<int>					indirect({1,1,1}, MEM_WRITABLE|MEM_INDIRECTARG);
	static DataBufferT<DrawVerticesArgs>	indirect2({{0, -1, 0, 1}}, MEM_WRITABLE|MEM_INDIRECTARG);

#endif

	ShaderVal	vals[] = {
		{"diffuse_irradiance",	&diffuse_irradiance},
		{"num_lights",			&num_lights},
		{"fog_dir1",			&fog_dir},
		{"fog_col1",			&fog_col},

		{"view",				&view},
		{"projection",			&proj},
		{"viewProj",			&viewProj},
		{"iview",				&iview},

		{"world",				&world},
		{"worldView",			&worldView},
		{"worldViewProj",		&worldViewProj},

		{"tint",				&tint},
		{"shadowlight_dir",		&shadowlight_dir},
		{"shadowlight_col",		&shadowlight_col},
		{"light_ambient",		&light_ambient},

		{"tint",				&tint},
		{"point_size",			&point_size},
		{"glossiness",			&glossiness},
		{"flags",				&flags},
		{"visible_meshlets",	&visible_meshlets},
		{"meshlet_indices",		&meshlet_indices},
	};
	ShaderVals	shader_params(vals);

	int			nmats	= 0;
	float3x4	*mats	= 0;
	if (Pose *pose = Root()->Property<Pose>()) {
		pose->GetSkinMats(ctx.allocator().alloc<float3x4>(nmats));

		nmats	= pose->Count();
		mats	= pose->skinmats;

#if 1
		SetSkinning(mats, nmats);
#else
		dual_quaternion	dq[50];
		for (int i = 0; i < nmats; i++) {
			dq[i] = mats[i];
			uint8	p = pose->parents[i];
			if (p != Pose::INVALID) {
				if (cosang(dq[i].r, dq[p].r) < zero) {
					dq[i].r = - dq[i].r;
					dq[i].t = - dq[i].t;
				}
			}
		}
		ctx.SetVertexShaderConstants(32, nmats * 2, (float*)dq);
#endif
	}

	ctx.SetBackFaceCull(cull);
	switch (flags.get_field(MODE)) {
		case POINTS: {
			Set(ctx, *ISO::root("data")["default"]["thickpoint"][HasIntVerts(model->submeshes[0])], ISO::MakeBrowser(shader_params));
			ctx.SetBlendEnable(true);
			for (SubMeshBase *sm : model->submeshes) {
				pass	*p = (*sm->technique)[0];
				if (p->Has(SS_VERTEX)) {
					auto		*rd = ((SubMeshPlat*)sm)->GetRenderData();
					ctx.SetVertexType(rd->vd);
					ctx.SetVertices(0, rd->vb, rd->vert_size);
					ctx.DrawPrimitive(PRIM_POINTLIST, 0, rd->nverts);
				}
			}
			break;
		}

		case WIREFRAME:
			ctx.SetFillMode(FILL_WIREFRAME);

		case FILL:
			if (model) {
				for (SubMeshBase *sm : model->submeshes) {
					pass	*p = (*sm->technique)[0];
				#if 0
					Set(ctx, p, ISO::MakeBrowser(ISO::combine(ISO::Browser(sm->parameters), ISO::MakeBrowser(shader_params))));
					((SubMeshPlat*)sm)->Render(ctx);
					if (!p->Has(SS_VERTEX)) {
						Set(ctx, (*sm->technique)[2], ISO::MakeBrowser(ISO::combine(ISO::Browser(sm->parameters), ISO::MakeBrowser(shader_params))));
						auto	verts	= ((SubMesh*)sm)->verts;
						auto	*rd		= (SubMeshPlat::renderdata*)verts.User().get();
						ctx.SetBuffer(SS_VERTEX, rd->vb, 0);
						ctx.SetBuffer(SS_VERTEX, meshlet_indices, 1);
						ctx.DrawVertices(PRIM_TRILIST, 0, meshlet_indices.Size() * 3);

					}
				#elif 0
					Set(ctx, p, ISO::MakeBrowser(ISO::combine(ISO::Browser(sm->parameters), ISO::MakeBrowser(shader_params))));
					((SubMeshPlat*)sm)->Render(ctx);
					if (!p->Has(SS_VERTEX)) {
						Set(ctx, (*sm->technique)[1], ISO::MakeBrowser(ISO::combine(ISO::Browser(sm->parameters), ISO::MakeBrowser(shader_params))));
						ctx.Dispatch(visible_meshlets.Size());

						ctx.PutCount(meshlet_indices, indirect2, 0);
						Set(ctx, (*sm->technique)[2], ISO::MakeBrowser(ISO::combine(ISO::Browser(sm->parameters), ISO::MakeBrowser(shader_params))));
						auto	verts	= ((SubMesh*)sm)->verts;
						auto	*rd		= (SubMeshPlat::renderdata*)verts.User().get();
						ctx.SetBuffer(SS_GEOMETRY, rd->vb, 0);
						ctx.SetBuffer(SS_GEOMETRY, meshlet_indices, 1);
						ctx.DrawVertices(PRIM_POINTLIST, indirect2, 0);

					}
				#else
					Set(ctx, p, ISO::MakeBrowser(ISO::combine(ISO::Browser(sm->parameters), ISO::MakeBrowser(shader_params))));
					((SubMeshPlat*)sm)->Render(ctx);

					if (!p->Has(SS_VERTEX)) {
						ctx.PutCount(visible_meshlets, indirect, 0);
					#if 0
						ctx.PutFence().Wait();
						auto	data = visible_meshlets.Data();
						dynamic_bitarray<>	b(((SubMesh*)sm)->indices.size(), false);
						for (auto i : data)
							ISO_ASSERT(!b[i].test_set() || i == 0);
					#endif

						Set(ctx, (*sm->technique)[1], ISO::MakeBrowser(ISO::combine(ISO::Browser(sm->parameters), ISO::MakeBrowser(shader_params))));
						ctx.Dispatch(indirect, 0);

						ctx.PutCount(meshlet_indices, indirect2, 0);
					#if 0
						ctx.PutFence().Wait();
						auto	data = meshlet_indices.Data();
						dynamic_bitarray<>	b(((SubMesh*)sm)->NumVerts(), false);
						for (auto &i : data) {
							b[i[0]] = true;
							b[i[1]] = true;
							b[i[2]] = true;
						}
					#endif

						Set(ctx, (*sm->technique)[2], ISO::MakeBrowser(ISO::combine(ISO::Browser(sm->parameters), ISO::MakeBrowser(shader_params))));
						auto	verts	= ((SubMesh*)sm)->verts;
						auto	*rd		= (SubMeshPlat::renderdata*)verts.User().get();
						ctx.SetBuffer(SS_GEOMETRY, rd->vb, 0);
						ctx.SetBuffer(SS_GEOMETRY, meshlet_indices, 1);
						ctx.DrawVertices(PRIM_POINTLIST, indirect2, 0);
					}
				#endif
				}

			} else if (patch) {
				tess_info.Calculate(worldViewProj, 16, tess);
				Draw(ctx, patch, tess.begin());

			} else if (nurbs) {
				for (int i = 0, n = nurbs->subpatches.Count(); i < n; i++) {
					SubPatch		&subpatch	= nurbs->subpatches[i];
					Set(ctx, (*subpatch.technique)[0], ISO::MakeBrowser(ISO::combine(ISO::Browser(subpatch.parameters), ISO::MakeBrowser(shader_params))));
					Draw(ctx, subpatch);
				}
			}

			ctx.SetFillMode(FILL_SOLID);

	}

	if (sel.mode) {
		ctx.SetBackFaceCull(BFC_NONE);
		ctx.SetDepthTestEnable(false);
		ctx.SetBlendEnable(true);
#ifndef USE_DX11
		ctx.SetAlphaTestEnable(false);
#endif

		if (model) {
			tint = {1,1,1};
			DrawSelection(ctx, sel, ISO::MakeBrowser(shader_params));

		} else if (patch) {
#if 0
			if (sel_submesh >= 0 && sel_index < 0) {
				ISO_ptr<SubPatch>	smp = patch->subpatches[sel_submesh];
				ShaderConstants		*sc = (ShaderConstants*)smp.User();
				sc[1].Set((*smp->technique)[1]);
				ctx.SetFillMode(FILL_SOLID);
				ctx.SetBackFaceCull(BFC_NONE);
				ctx.SetDepthTestEnable(false);
				ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_ONE);
				ctx.SetBlendEnable(true);
				ctx.SetAlphaTestEnable(false);

				ctx.Device()->SetRenderState(D3DRS_POINTSIZE, iorf(8.f).i);
				ctx.DrawPrimitive(PRIM_POINTLIST, ~sel_index, 1);
			}
#endif
		}

	}

	if (flags.test(BONES)) {
		ctx.SetBackFaceCull(BFC_NONE);
		ctx.SetDepthTestEnable(false);
		ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_ONE);
		ctx.SetBlendEnable(true);
#ifndef USE_DX11
		ctx.SetAlphaTestEnable(false);
#endif

		world			= Root()->GetWorldMat();
		worldView		= view * world;
		worldViewProj	= viewProj * world;

		Set(ctx, *ISO::root("data")["default"]["blend_vc"][0], ISO::MakeBrowser(shader_params));
		Pose*		pose	= Root()->Property<Pose>();

		for (int i = 0; i < pose->Count(); i++) {
			float3x4	mat		= pose->skinmats[i] * inverse(pose->invbpmats[i]);
			position3	pos		= get_trans(mat);
			int			parent	= pose->parents[i];
			if (parent != Pose::INVALID) {
#ifndef USE_DX11
				ctx.Device()->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, TRUE);
#endif
				ImmediateStream<vertex>	ims(ctx, PRIM_LINELIST, 2);
				vertex	*p = ims.begin();
				p[0].set((pose->skinmats[parent] * inverse(pose->invbpmats[parent])).w, colour(1,1,1));
				p[1].set(pos, colour(1,1,1));
			}
		}

		if (sel.mode == Selection::BONE) {
			float3x4	mat		= pose->skinmats[sel.index] * inverse(pose->invbpmats[sel.index]);
			position3	pos		= get_trans(mat);
			int			parent	= pose->parents[sel.index];
			if (parent != Pose::INVALID) {
#ifndef USE_DX11
				ctx.Device()->SetRenderState(D3DRS_ANTIALIASEDLINEENABLE, TRUE);
#endif
				ImmediateStream<vertex>	ims(ctx, PRIM_LINELIST, 2);
				vertex	*p = ims.begin();
				p[0].set((pose->skinmats[parent] * inverse(pose->invbpmats[parent])).w, colour(1,1,1));
				p[1].set(pos, colour(1,1,1));
			}

			{
#ifndef USE_DX11
				ctx.Device()->SetRenderState(D3DRS_POINTSIZE, iorf(8.f).i);
#endif
				ImmediateStream<vertex>	ims(ctx, PRIM_POINTLIST, 1);
				ims.begin()->set(pos, colour(1,1,1));
			}

			DrawMatrix(ctx, mat, specular, ISO::MakeBrowser(shader_params));
		}

	} else if (sel_mat) {
		ctx.SetBackFaceCull(BFC_NONE);
		ctx.SetDepthTestEnable(false);
		ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_ONE);
		ctx.SetBlendEnable(true);
#ifndef USE_DX11
		ctx.SetAlphaTestEnable(false);
#endif
		Set(ctx, *ISO::root("data")["default"]["blend_vc"][0], ISO::MakeBrowser(shader_params));

		DrawMatrix(ctx, *sel_mat, specular, ISO::MakeBrowser(shader_params));
	}

	if (hover.mode) {
		ctx.SetBackFaceCull(BFC_NONE);
		ctx.SetDepthTestEnable(false);
		ctx.SetBlendEnable(true);
	#ifndef USE_DX11
		ctx.SetAlphaTestEnable(false);
	#endif
		tint = {1,0.5f,0};
		DrawSelection(ctx, hover, ISO::MakeBrowser(shader_params));
	}

	float3x4	orig_world		= world;

	static pass *thickline		= *ISO::root("data")["default"]["thicklineR"][0];
#if 0
	world			= scale(move_scale) * orig_world;
	worldView		= view * world;
	worldViewProj	= viewProj * world;
	Set(ctx, coloured);
	DrawWireFrameGrid(ctx, 20, 20);

	world			= scale(move_scale) * orig_world * rotate_in_x(pi / 2);
	worldView		= view * world;
	worldViewProj	= viewProj * world;
	Set(ctx, coloured);
	DrawWireFrameGrid(20, 20);

	world			= scale(move_scale) * orig_world * rotate_in_y(pi / 2);
	worldView		= view * world;
	worldViewProj	= viewProj * world;
	Set(ctx, coloured);
	DrawWireFrameGrid(20, 20);
#endif

	if (flags.test(BOUNDING_EDGES)) {
		ctx.SetBackFaceCull(BFC_NONE);
		ctx.SetDepthTestEnable(true);
		ctx.SetBlend(BLENDOP_ADD, BLEND_ONE, BLEND_INV_SRC_ALPHA);
		ctx.SetBlendEnable(true);

		cuboid		extent;
		if (model) {
			extent = cuboid(position3(model->minext), position3(model->maxext));
		} else if (patch) {

		}

		worldViewProj	= viewProj * world * extent.matrix();
		tint	= colour(0,1,0);
		Set(ctx, thickline, ISO::MakeBrowser(shader_params));
		DrawWireFrameBox(ctx);
	}

	ctx.SetWindow(GetClientRect().Subbox(0, -100, 100, 0));
	ctx.SetBlendEnable(false);
	ctx.SetDepthTestEnable(true);

	proj			= hardware_fix(parallel_projection(0.7f, 0.7f, -10.f, 10.f));
	viewProj		= proj * float3x3(view);
	iview			= (float3x4)float3x3(inverse(view));
	shadowlight_dir	= iview * float3{0, 0, -1};
	shadowlight_col	= colour(1, 1, 1);
	DrawAxes(ctx, .05f, .2f, .2f, 0.25f, specular, identity, ISO::MakeBrowser(shader_params));
}

void ViewModel::DrawMatrix(GraphicsContext &ctx, param(float3x4) mat, pass *p, ISO::Browser params) {
#if 1
	position3	middle = view_loc * get_trans(mat);
	DrawAxes(ctx, .05f, .2f, .2f, 0.25f, p, mat * scale(float(middle.v.z) / 8), params);
#else
	ImmediateStream<vertex>	ims(ctx, PRIM_LINELIST, 6);
	vertex	*p = ims.begin();
	float	bone_size = move_scale / 8;
	p[0].set(mat.w, colour(1,0,0));
	p[1].set(mat.w + normalise(mat.x.xyz) * bone_size, colour(1,0,0));
	p[2].set(mat.w, colour(0,1,0));
	p[3].set(mat.w + normalise(mat.y.xyz) * bone_size, colour(0,1,0));
	p[4].set(mat.w, colour(0,0,1));
	p[5].set(mat.w + normalise(mat.z.xyz) * bone_size, colour(0,0,1));
#endif
}

template<> static const VertexElements ve<ViewModel::vertex> = (const VertexElement[]) {
	{&ViewModel::vertex::pos, "position"_usage},
	{&ViewModel::vertex::col, "colour"_usage}
};

ViewModel::ViewModel(MainWindow &_main, const WindowPos &wpos, ISO_ptr_machine<void> p)
	: main(_main), cull(BFC_BACK), flags(FILL), original(p)
	, view_loc(identity), target_loc(identity), focus_pos(zero)
	, light(identity), light_ambient(0.25f, 0.25f, 0.25f), anim(0)
{
	Texture	*refmap = GetShaderParameter<crc32_const("_refmap")>();
	if (!*refmap)
//		(ISO_ptr_machine<void>&)*refmap = ISO::root("data")["refmap"];
		*refmap = force_cast<Texture>(ISO::root("data")["refmap"].GetPtr());

	Use();

	if (p.GetType() == ISO::getdef<Model>()) {
		model = ISO_ptr<Model3>(ISO::FileHandler::ExpandExternals(p));

	} else if (p.GetType() == ISO::getdef<Model3>()) {
		ISO::Type	*type = ISO::getdef<Model3>();
		save(type->flags, type->flags & ~ISO::TypeUser::CHANGE),
			model = ISO_conversion::convert<Model3>(p, ISO_conversion::RECURSE | ISO_conversion::EXPAND_EXTERNALS | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK);//p;

		for (SubMesh *sm : model->submeshes) {
			if (!sm->technique) {
				if (sm->VertComponent(USAGE_NORMAL))
					sm->technique = ISO::root("data")["default"]["specular"];
			}
		}

	} else if (p.GetType() == ISO::getdef<SubMesh>()) {
		ISO_ptr<SubMesh>	sm = FileHandler::ExpandExternals(p);
		model.Create(0);
		model->submeshes.Append(sm);
		model->minext = sm->minext;
		model->maxext = sm->maxext;

	} else if (p.GetType() == ISO::getdef<PatchModel3>()) {
		patch = p;

	} else if (p.GetType() == ISO::getdef<NurbsModel>()) {
		nurbs = p;

	} else if (p.GetType()->SameAs<BasePose>()) {
		Pose	*pose = Pose::Create(basepose = p);
		Root()->SetProperty(pose);
		flags.set(BONES);

	} else if (p.GetType()->Is("MergeModels")) {
		ISO_ptr<anything> a = ISO_conversion::convert(p, NULL, ISO_conversion::RECURSE | ISO_conversion::EXPAND_EXTERNALS | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK);
		for (int i = 0, n = a->Count(); i < n; i++) {
			if ((*a)[i].GetType()->SameAs<Model3>()) {
				model = (*a)[i];
				break;
			}
		}

	} else {
		ISO::Type	*type = ISO::getdef<Model3>();
		save(type->flags, type->flags & ~ISO::TypeUser::CHANGE),
			node = ISO_conversion::convert<Node>(p, ISO_conversion::RECURSE | ISO_conversion::EXPAND_EXTERNALS | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK);

		for (int i = 0, n = node->children.Count(); i < n; i++) {
			ISO_ptr<void>	&child = node->children[i];
			const ISO::Type *type = child.GetType();

			if (type->SameAs<BasePose>()) {
				Pose	*pose = Pose::Create(basepose = child);
				Root()->SetProperty(pose);

			} else if (type == ISO::getdef<Model3>()) {
				model = child;

			} else if (type == ISO::getdef<PatchModel3>()) {
				patch = child;

			} else if (type == ISO::getdef<NurbsModel>()) {
				nurbs = child;
			}
		}
	}

	bone_texture.Init(1024 * 4, 1, 1, 1, MEM_CPU_WRITE);
	{
		aligned<float4x4,16>	*mats	= new aligned<float4x4,16>[1024];
		for (int i = 0; i < 1024; i++)
			mats[i] = identity;
		memcpy(bone_texture.WriteData()[0].begin(), mats, 1024 * sizeof(float4x4));
		delete[] mats;
	}

	if (model || patch || nurbs) {
		cuboid	ext = model	? cuboid(position3(model->maxext), position3(model->minext))
					: patch ? cuboid(position3(patch->maxext), position3(patch->minext))
					: cuboid(position3(nurbs->maxext), position3(nurbs->minext));
		focus_pos	= ext.centre();
		sel_loc		= rot_trans(identity, focus_pos);
		move_scale	= len(ext.extent());

		if (patch) {
			tess_info.Init(patch);
			tess.Create(tess_info.Total());
		}
	}

	addref();
	if (model) {
		addref();
		RunThread([this]() {
			bool	convert_verts	= false;
			size_t	total_verts		= 0;
			for (SubMesh *sm : model->submeshes) {
				convert_verts = convert_verts || !sm->VertComponent(0)->type->SameAs<float3p>();
				total_verts += sm->NumVerts();
			}

			dynamic_array<cuboid>	exts;

			if (convert_verts) {
				verts.resize(total_verts);
				total_verts		= 0;

				for (SubMesh *sm : model->submeshes) {
					auto	comp	= sm->VertComponent(0);
					auto	v		= verts + total_verts;
					ISO::Conversion::batch_convert(sm->_VertComponentData(comp->offset), comp->type, v, sm->NumVerts());
					total_verts += sm->NumVerts();

					for (auto &f : sm->indices)
						exts.push_back(get_extent(make_indexed_container(v, f)));
				}
			} else {
				for (auto &smp : model->submeshes) {
					auto	isize	= smp.GetType()->SkipUser()->as<ISO::COMPOSITE>()->Find("indices")->type->as<ISO::OPENARRAY>()->subsize / 4;
					if (isize == 3) {
						SubMesh	*sm		= smp;
						auto	v		= sm->VertComponentData<float3p>(0);
						for (auto &f : sm->indices)
							exts.push_back(get_extent(make_indexed_container(v, f)));
					}
				}
			}
			oct.init(exts, exts.size32());
			release();
		});
	}

	near_z			= move_scale / 100;
	view_loc		= rot_trans(rotate_in_x(pi * half), position3(0, 0, move_scale / 2) - view_loc.rot * float3(focus_pos));
	target_loc		= view_loc;

	Create(wpos, get_id(p), CHILD | VISIBLE | CLIPCHILDREN | CLIPSIBLINGS, CLIENTEDGE);
}


class EditorModel : public Editor {
	bool	Matches(const ISO::Type *type) override {
		return type == ISO::getdef<Model3>()
			|| type == ISO::getdef<Model>()
			|| type == ISO::getdef<SubMesh>()
			|| type == ISO::getdef<PatchModel3>()
			|| type == ISO::getdef<NurbsModel>()
			|| type->SameAs<Node>()
			|| type->SameAs<BasePose>()
			|| type->Is("MergeModels");
	}
	Control	Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr_machine<void> &p) override {
		return *new ViewModel(main, wpos, p);
	}
	ID		GetIcon(const ISO_ptr<void> &p)	override {
		return "IDB_DEVICE_LANDSCAPE";
	}

} editormodel;

static initialise init(
	ISO::getdef<fx>(),
	ISO::getdef<Scene>(),
	ISO::getdef<Animation>(),
	ISO::getdef<AnimationEventKey>(),
	ISO::getdef<BasePose>(),
	ISO::getdef<Collision_OBB>(),
	ISO::getdef<Collision_Sphere>(),
	ISO::getdef<Collision_Cylinder>(),
	ISO::getdef<Collision_Capsule>(),
	ISO::getdef<Collision_Patch>(),
	ISO::getdef<ent::Light2>(),
	ISO::getdef<ent::Attachment>()
);