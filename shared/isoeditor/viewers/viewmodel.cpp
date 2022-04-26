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

position3 GetVertex(SubMesh *sm, int i) {
	auto	v	= sm->VertComponentData<float3p>(0);
	return position3(v[i]);
}

triangle3 GetTriangle(SubMesh *sm, int i) {
	auto	&f	= sm->indices[i];
	auto	v	= sm->VertComponentData<float3p>(0);
	return triangle3(position3(v[f[0]]), position3(v[f[1]]), position3(v[f[2]]));
}

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

class ViewModel : public Window<ViewModel>, public WindowTimer<ViewModel>, public World, Graphics::Display {

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

	MainWindow				&main;
	Accelerator				accel;
	BackFaceCull			cull;
	iso::flags<FLAGS>		flags;
	ToolBarControl			toolbar;
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

	enum {
		NONE, VERTEX, FACE, PATCH, SUBMESH//, MATRIX, BONE
	} sel_mode;
	int						sel_submesh, sel_patch, sel_index, sel_bone;
	rot_trans				sel_loc;
	float3x4p*				sel_mat;

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

	void			DrawScene(GraphicsContext &ctx);
//	void			DrawMatrix(GraphicsContext &ctx, param(float3x4) mat);
	void			DrawMatrix(GraphicsContext &ctx, param(float3x4) mat, pass *p, ISO::Browser params);
	void			ResetView();

	bool			ClickAxes(const point &mouse);
	bool			ClickModel(const point &mouse);

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

	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_CREATE: {
				toolbar.Create(*this, NULL, CHILD | CLIPSIBLINGS | VISIBLE | toolbar.NORESIZE | toolbar.FLAT | toolbar.TOOLTIPS);
				toolbar.Init("IDR_TOOLBAR_MESH");
//				toolbar.Add(Menu("IDR_MENU_VIEWMODEL"));
				toolbar.CheckButton(ID_MESH_BACKFACE, cull == BFC_FRONT);
				toolbar.CheckButton(ID_MESH_FILL, flags.test(FILL));

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

//					ctx.SetDepthTestEnable(false);
//					ctx.SetRenderTarget(Surface(), RT_DEPTH);
//					ctx.SetRenderTarget(GetDispSurface());
//					PostEffects(ctx).FullScreenQuad((*PostEffects::shaders->copy)[0]);

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
					if (ClickAxes(prevmouse) || ClickModel(prevmouse))
						Invalidate();
				}
				break;

			case WM_LBUTTONDBLCLK:
				//ChangeView(translate(zero, zero, sqrt(sel_loc.trans4.w) * two) / sel_loc);
				ChangeView(rot_trans(rotate_in_x(pi * half), position3(zero, zero, move_scale / 2)));
				break;

			case WM_MOUSEMOVE: {
				Point		mouse(lParam);
				if (wParam & MK_MBUTTON) {
					if (sel_bone >= 0) {
						Pose		*pose	= Root()->Property<Pose>();
						float3x4	wmat	= pose->GetObjectMatrix(sel_bone);
						float3		axis	= wmat * float3{float(prevmouse.y - mouse.y), 0, float(prevmouse.x - mouse.x)};
						quaternion	q(normalise(concat(axis, 512)));
						pose->joints[sel_bone].rot *= q;
						pose->mats[sel_bone] = pose->joints[sel_bone];
						pose->Update();
						Invalidate();
					}
				} else if (wParam & MK_RBUTTON) {
					StopMoving();
					view_loc *= translate(float3{float(mouse.x - prevmouse.x), float(mouse.y - prevmouse.y), 0} * (move_scale / 256));
					Invalidate();

				} else if (wParam & MK_LBUTTON) {
					if (sel_bone >= 0) {
						Pose *pose = Root()->Property<Pose>();
						pose->mats[sel_bone] = pose->mats[sel_bone] * rotate_in_y((mouse.x - prevmouse.x) / 100.f) * rotate_in_x((mouse.y - prevmouse.y) / 100.f);

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
				}
				prevmouse	= mouse;
				break;
			}

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
					if (sel_mode == FACE) {
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
						ISO::Browser b	= *(ISO::Browser*)lParam;
						const ISO::Type	*type = b.GetTypeDef();
						if (type->SkipUser()->GetType() == ISO::REFERENCE) {
							b		= *b;
							type	= b.GetTypeDef();
							if (type->Is("Animation")) {
								if (anim)
									delete anim;
								anim = new AnimationHolder(Root(), b);
								Timer::Start(1/60.f);
								time.reset();
								return TRUE;

							} else if (type->Is("AnimationHierarchy")) {
								Root()->AddEntities(b);

							} else if (type->SameAs<Bone>()) {
								if (BasePose *bp = basepose) {
									Bone	*bone = b;
									for (int i = 0, n = bp->Count(); i < n; i++) {
										if (bone == (*bp)[i]) {
											sel_mat		= 0;
											flags.set(BONES);
											sel_bone	= i;
											Invalidate();
											return true;
										}
									}
								}

							} else if (type->SameAs<SubMesh>()) {
								if (model && (FindSubMesh(model, b, sel_submesh) || (original.IsType<Model3>() && FindSubMesh((Model3*)original, b, sel_submesh)))) {
									sel_mode	= SUBMESH;
									Invalidate();
									return true;
								}
							}

						} else if (model && type->GetType() == ISO::ARRAY && type->SubType()->SameAs<uint16>()) {
							if (FindIndex(model, b, sel_submesh, sel_index)
							|| (original.IsType<Model3>() && FindIndex((Model3*)original, b, sel_submesh, sel_index))
							) {
								sel_mode	= FACE;
								sel_loc		= as_rot_trans(GetTriangle(model->submeshes[sel_submesh], sel_index));
								Invalidate();
								return true;
							}
							if (ISO::Browser bi	= ISO::Browser(original)["indices"]) {
								if (FindIndex(bi, b, sel_index)) {
									sel_mode	= FACE;
									sel_loc		= as_rot_trans(GetTriangle(model->submeshes[sel_submesh], sel_index));
								}
								Invalidate();
								return true;
							}

						} else if (type->SameAs<float3x4p>()) {
							sel_mat		= (float3x4p*)b;
							sel_bone	= -1;
							Invalidate();
							return true;

						} else if (model) {
							if (FindVertex(model, b, sel_submesh, sel_index) || (original.IsType<Model3>() && FindVertex((Model3*)original, b, sel_submesh, sel_index))) {
								sel_mode	= VERTEX;
								sel_loc		= rot_trans(identity, GetVertex(model->submeshes[sel_submesh], sel_index));
								Invalidate();
								return true;
							}

						} else if (patch) {
							if (FindVertex(patch, b, sel_submesh, sel_patch, sel_index) || (original.IsType<PatchModel3>() && FindVertex((PatchModel3*)original, b, sel_submesh, sel_patch, sel_index))) {
								sel_mode	= sel_index < 16 ? VERTEX : PATCH;
								Invalidate();
								return true;
							}
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

	ViewModel(MainWindow &_main, const WindowPos &wpos, const ISO_ptr_machine<void> p);
};

void ViewModel::ResetView() {
	//InitTimer(0.01f);
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


bool ViewModel::ClickModel(const point &mouse) {
	if (model) {
		if (!oct.nodes) {
			dynamic_array<cuboid>	exts;
			for (SubMesh *sm : model->submeshes) {
				auto	v = sm->VertComponentData<float3p>(0);
				for (auto &f : sm->indices)
					exts.push_back(get_extent(make_indexed_container(v, f)));
			}
			oct.init(exts, exts.size32());
		}

		int			total	= 0;
		dynamic_array<int>	sm_sizes(transformc(model->submeshes, [&total](SubMesh *sm) { return exchange(total, total + sm->indices.size32()); }));

		float4x4	mat		= translate(-mouse.x, -mouse.y, 0) * ScreenMat();

		int			face	= oct.shoot_ray(mat, 0.25f, [&](int i, param(ray3) r, float &t) {
			auto		smi	= upper_boundc(sm_sizes, i) - 1;
			triangle3	tri	= GetTriangle(model->submeshes[sm_sizes.index_of(smi)], i - *smi);
			float3		normal;
			return tri.ray_check(r, t, &normal) && dot(normal, r.d) < zero;
		});

		if (face >= 0) {
			auto	smi		= upper_boundc(sm_sizes, face) - 1;
			sel_submesh		= sm_sizes.index_of(smi);
			face -= *smi;

			float	mind	= 10;
#if 0
			SubMesh		*sm		= model->submeshes[sel_submesh];
			triangle3	tri		= GetTriangle(sm, face);
			auto		index	= sm->indices[face].begin();
			for (auto &p : tri.corners()) {
				float		d	= len2(project(mat * float4(p)).xy);
				if (d < mind) {
					mind		= d;
					sel_index	= *index;
					sel_mode	= VERTEX;
					sel_loc		= p;
				}
				++index;
			}
			if (mind == 10) {
				sel_loc		= tri.centre();
				sel_index	= face;
				sel_mode	= FACE;
			}
#else
			float3		centre(zero);
			SubMesh		*sm	= model->submeshes[sel_submesh];
			auto		&f	= sm->indices[face];
			auto		v	= sm->VertComponentData<float3p>(0);

			for (int i = 0; i < 3; i++) {
				position3	p	= position3(v[f[i]]);
				float		d	= len2(project(mat * float4(p)).v.xy);
				if (d < mind) {
					mind		= d;
					sel_index	= f[i];
					sel_mode	= VERTEX;
					sel_loc		= rot_trans(identity, p);
				}
				centre += p.v;
			}
			if (mind == 10) {
				sel_loc		= as_rot_trans(GetTriangle(sm, face));	//	centre / 3;
				sel_index	= face;
				sel_mode	= FACE;
			}
#endif
			if (original.IsType<Model3>()) {
				string_builder	route;
				if (((IsoEditor&)main).LocateRoute(route, ((Model3*)original)->submeshes[sel_submesh]))
					((IsoEditor&)main).SelectRoute(route << "[" << (sel_mode == FACE ? 2 : 1) << "][" << sel_index << ']');
			}

			return true;
		}
	}
	return false;
}


void ViewModel::DrawScene(GraphicsContext &ctx) {
	static pass *specular		= *ISO::root("data")["default"]["specular"][0];

#if 0
	float4x4	in(
		position3(0,0,0),
		position3(-.48f - 1,-.30f,1),
		position3(+.48f - 1,-.30f,1),
		position3(-.48f - 1,+.30f,1)
	);
	float4x4	out(
		float4(0,0,-1,0),
		float4(-1,-1,0,1),
		float4(+1,-1,0,1),
		float4(-1,+1,0,1)
	);
	float4x4	proj0			= out * inverse(in);
//	float4x4	proj0			= find_projection(in, out);
#else
	float4x4	proj0			= Projection();
#endif

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
	hash_map<crc32, arbitrary_ptr> shader_params;

	shader_params["diffuse_irradiance"]	= &diffuse_irradiance;
	shader_params["num_lights"]			= &num_lights;
	shader_params["fog_dir1"]			= &fog_dir;
	shader_params["fog_col1"]			= &fog_col;

	shader_params["view"]				= &view;
	shader_params["projection"]			= &proj;
	shader_params["viewProj"]			= &viewProj;
	shader_params["iview"]				= &iview;

	shader_params["world"]				= &world;
	shader_params["worldView"]			= &worldView;
	shader_params["worldViewProj"]		= &worldViewProj;

	shader_params["tint"]				= &tint;
	shader_params["shadowlight_dir"]	= &shadowlight_dir;
	shader_params["shadowlight_col"]	= &shadowlight_col;
	shader_params["light_ambient"]		= &light_ambient;

	shader_params["tint"]		= &tint;
	shader_params["point_size"]			= &point_size;
#else
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
	};
	ShaderVals	shader_params(vals);
#endif

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
		case POINTS:
			Set(ctx, *ISO::root("data")["default"]["thickpoint"][0], ISO::MakeBrowser(shader_params));
			for (int i = 0, n = model->submeshes.Count(); i < n; i++) {
				SubMeshBase	*sm = model->submeshes[i];
				auto		*rd = ((SubMeshPlat*)sm)->GetRenderData();
				ctx.SetVertexType(rd->vd);
				ctx.SetVertices(0, rd->vb, rd->vert_size);
				ctx.DrawPrimitive(PRIM_POINTLIST, 0, rd->nverts);
			}
			break;

		case WIREFRAME:
			ctx.SetFillMode(FILL_WIREFRAME);

		case FILL:
			if (model) {
				for (int i = 0, n = model->submeshes.Count(); i < n; i++) {
					SubMeshBase	*sm = model->submeshes[i];
					Set(ctx, (*sm->technique)[0], ISO::MakeBrowser(ISO::combine(ISO::Browser(sm->parameters), ISO::MakeBrowser(shader_params))));
					((SubMeshPlat*)sm)->Render(ctx);
				}
			} else if (patch) {
				tess_info.Calculate(worldViewProj, 16, tess);
				Draw(ctx, patch, tess.begin());

			} else if (nurbs) {
		#if 1
				for (int i = 0, n = nurbs->subpatches.Count(); i < n; i++) {
					SubPatch		&subpatch	= nurbs->subpatches[i];
					Set(ctx, (*subpatch.technique)[0], ISO::MakeBrowser(ISO::combine(ISO::Browser(subpatch.parameters), ISO::MakeBrowser(shader_params))));
					Draw(ctx, subpatch);
				}
		#else
				AddShaderParameter<crc32_const("view")				>(view);
				AddShaderParameter<crc32_const("projection")		>(proj);
				AddShaderParameter<crc32_const("viewProj")			>(viewProj);
				AddShaderParameter<crc32_const("iview")				>(iview);
				AddShaderParameter<crc32_const("world")				>(world);
				AddShaderParameter<crc32_const("worldView")			>(worldView);
				AddShaderParameter<crc32_const("worldViewProj")		>(worldViewProj);
				AddShaderParameter<crc32_const("tint")				>(tint);
				AddShaderParameter<crc32_const("shadowlight_dir")	>(shadowlight_dir);
				AddShaderParameter<crc32_const("shadowlight_col")	>(shadowlight_col);
				AddShaderParameter<crc32_const("light_ambient")		>(light_ambient);

				Draw(ctx, nurbs);
		#endif
			}

			ctx.SetFillMode(FILL_SOLID);

	}

	if (sel_mode != NONE) {
		ctx.SetBackFaceCull(BFC_NONE);
		ctx.SetDepthTestEnable(false);
		ctx.SetBlendEnable(false);
#ifndef USE_DX11
		ctx.SetAlphaTestEnable(false);
#endif

		if (model) {
			ISO_ptr<SubMeshBase>	smb = model->submeshes[sel_submesh];
			SubMeshPlat::renderdata	*rd = (SubMeshPlat::renderdata*)((SubMeshPlat&)*smb).verts.User().get();
			ctx.SetVertexType(rd->vd);
			ctx.SetVertices(0, rd->vb, rd->vert_size);
			ctx.SetIndices(rd->ib);

			switch (sel_mode) {
				case VERTEX: {
#ifndef USE_DX11
					ctx.Device()->SetRenderState(D3DRS_POINTSIZE, iorf(8.f).i);
#else
					Set(ctx, *ISO::root("data")["default"]["thickpoint"][0], ISO::MakeBrowser(shader_params));
#endif
					ctx.DrawPrimitive(PRIM_POINTLIST, sel_index, 1);
					break;
				}
				case FACE: {
					uint8	vpp		= smb->GetVertsPerPrim();
					//int		t		= 0;//smb->technique->Count() > 1 ? 1 : 0;
					//Bind(model)[sel_submesh * 4 + t].Set(ctx, (*smb->technique)[t]);
					Set(ctx, *ISO::root("data")["default"]["coloured"][0], ISO::MakeBrowser(shader_params));
					ctx.DrawIndexedVertices(PrimType(rd->prim), 0, rd->nverts, sel_index * vpp, vpp);
					break;
				}
				case SUBMESH: {
					//int		t		= 0;//smb->technique->Count() > 1 ? 1 : 0;
					//Bind(model)[sel_submesh * 4 + t].Set(ctx, (*smb->technique)[t]);
					Set(ctx, *ISO::root("data")["default"]["coloured"][0], ISO::MakeBrowser(shader_params));
					ctx.DrawIndexedVertices(PrimType(rd->prim), 0, rd->nverts, 0, rd->nindices);
					break;
				}
			}

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

		if (sel_bone >= 0) {

			float3x4	mat		= pose->skinmats[sel_bone] * inverse(pose->invbpmats[sel_bone]);
			position3	pos		= get_trans(mat);
			int			parent	= pose->parents[sel_bone];
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

namespace iso {
template<> VertexElements GetVE<ViewModel::vertex>() {
	static VertexElement ve[] = {
		VertexElement(&ViewModel::vertex::pos, "position"_usage),
		VertexElement(&ViewModel::vertex::col, "colour"_usage)
	};
	return ve;
};
}

ViewModel::ViewModel(MainWindow &_main, const WindowPos &wpos, ISO_ptr_machine<void> p)
	: main(_main), cull(BFC_BACK), flags(FILL), original(p)
	, sel_mode(NONE), sel_bone(-1), sel_mat(0)
	, view_loc(identity), target_loc(identity), focus_pos(zero)
	, light(identity), light_ambient(0.25f, 0.25f, 0.25f), anim(0)
{
	Texture	*refmap = GetShaderParameter<crc32_const("_refmap")>();
	if (!*refmap)
//		(ISO_ptr_machine<void>&)*refmap = ISO::root("data")["refmap"];
		*refmap = force_cast<Texture>(ISO::root("data")["refmap"].GetPtr());

	Use();

//	try {
		if (p.GetType() == ISO::getdef<Model3>()) {
			ISO::Type	*type = ISO::getdef<Model3>();
			save(type->flags, type->flags & ~ISO::TypeUser::CHANGE),
				model = ISO_conversion::convert<Model3>(p, ISO_conversion::RECURSE | ISO_conversion::EXPAND_EXTERNALS | ISO_conversion::CHECK_INSIDE | ISO_conversion::FULL_CHECK);//p;

			for (auto &sm : model->submeshes) {
				if (!sm->technique) {
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
			memcpy(bone_texture.WriteData(), mats, 1024 * sizeof(float4x4));
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

		near_z			= move_scale / 100;
		view_loc		= rot_trans(rotate_in_x(pi * half), position3(0, 0, move_scale / 2) - view_loc.rot * float3(focus_pos));
		target_loc		= view_loc;
//	} catch (char *s) {
//		MessageBoxA(*this, s, "Error", MB_ICONERROR | MB_OK);
//	}

	Create(wpos, get_id(p), CHILD | VISIBLE | CLIPCHILDREN | CLIPSIBLINGS, CLIENTEDGE);
}


class EditorModel : public Editor {
	virtual bool Matches(const ISO::Type *type) {
		return type == ISO::getdef<Model3>()
			|| type == ISO::getdef<SubMesh>()
			|| type == ISO::getdef<PatchModel3>()
			|| type == ISO::getdef<NurbsModel>()
			|| type->SameAs<Node>()
			|| type->SameAs<BasePose>()
			|| type->Is("MergeModels");
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_ptr_machine<void> &p) {
		return *new ViewModel(main, wpos, p);
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