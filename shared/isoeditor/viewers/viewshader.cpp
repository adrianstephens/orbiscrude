#include "app.h"
#include "main.h"
#include "iso/iso_files.h"
#include "iso/iso_convert.h"
#include "graphics.h"
#include "shader.h"
#include "postprocess/post.h"
#include "maths/geometry.h"

using namespace app;

spherical_coord pow(param(spherical_coord) s, float n) {
	return spherical_coord(spherical_dir(s.v * n), pow(s.r, n));
}

float MandelBulb(param(float3) pos) {
	const int Iterations	= 12;
	const float Bailout		= 8;
	float Power				= 8;// + sin(time * 0.5) * 4;

	position3	z	= position3(pos);
	float		dr	= 1;
	spherical_coord	s;
	for (int i = 0; i < Iterations; i++) {
		s = spherical_coord(z);
		if (s.r > Bailout)
			break;

		dr	= pow(s.r, Power - 1) * Power * dr + 1;
		position3	p = pow(s, Power);
		z	= position3(p + pos);
	}
	return log(s.r) * half * s.r / dr;
}

float	GetDist(param(float3) pos) {
//	return MandelBulb(pos);
	return 1;
}

class ViewShader : public Window<ViewShader>, public WindowTimer<ViewShader> {
	unique_ptr<RenderOutput>	output;
	MainWindow			&main;
	timer				time;

	rot_trans			camera			= identity;
	rot_trans			object			= {identity, position3(0.0001f,0,4)};

	position3			light			= {0, 2, 0};
	colour				materialColour;

	int					buttons			= 0;
	Point				mouse;

	ISO_ptr<DX11Shader>	shader;

	void			DrawScene(GraphicsContext &ctx, const float3x4 &eye, const RenderView &rv);

	void ButtonDown(int b, LPARAM lParam) {
		mouse	= Point(lParam);
		buttons	|= b;
		((IsoEditor&)main).SetEditWindow(*this);
	}
	void ButtonUp(int b) {
		buttons	&= ~b;
	}

public:
	LRESULT Proc(UINT message, WPARAM wParam, LPARAM lParam) {
		switch (message) {
			case WM_SIZE:
				if (output)
					output->SetSize((RenderWindow*)hWnd, Point(lParam));
				break;

			case WM_PAINT: {
				static bool	drawing;
				if (drawing)
					return 0;

				drawing = true;

				DeviceContextPaint	dc(*this);
				if (quaternion *q = GetShaderParameter("orientation"))
					camera.rot = ~*q;
				if (position3 *p = GetShaderParameter("position"))
					camera.trans4 = -*p;
				try {
					GraphicsContext ctx;
					output->BeginFrame(ctx);
					ctx.SetBackFaceCull(BFC_NONE);
					ctx.SetDepthTestEnable(false);

					if (output->GetFlags() & RenderOutput::DIMENSIONS_3) {
						float	focus	= -GetDist(camera.trans4.xyz - object.trans4.xyz);
						DrawScene(ctx, camera, output->GetView(0));
						DrawScene(ctx, camera, output->GetView(1));

//						float	sep		= output->EyeSeparation() / 2;
//						ctx.SetWindow(output->EyeViewport(0));
//						DrawScene(ctx, camera * inverse(stereo_skew(-sep, focus)));
//						ctx.SetWindow(output->EyeViewport(1));
//						DrawScene(ctx, camera * inverse(stereo_skew(+sep, focus)));
					} else {
						DrawScene(ctx, camera, output->GetView(0));
					}
					output->EndFrame(ctx);
					drawing = false;
				} catch (char *s) {
					MessageBoxA(*this, s, "Graphics Error", MB_ICONERROR | MB_OK);
				}
				break;
			}

			case WM_LBUTTONDOWN:
				ButtonDown(MK_LBUTTON, lParam);
				break;
			case WM_RBUTTONDOWN:
				ButtonDown(MK_RBUTTON, lParam);
				break;
			case WM_MBUTTONDOWN:
				ButtonDown(MK_MBUTTON, lParam);
				break;

			case WM_LBUTTONUP:
				ButtonUp(MK_LBUTTON);
				break;
			case WM_RBUTTONUP:
				ButtonUp(MK_RBUTTON);
				break;
			case WM_MBUTTONUP:
				ButtonUp(MK_MBUTTON);
				break;

			case WM_MOUSEMOVE: {
				Point		new_mouse(lParam);
				float2		dm{float(mouse.x - new_mouse.x), float(mouse.y - new_mouse.y)};
				bool		obj = (wParam & MK_SHIFT) || GetShaderParameter("orientation");
				if (buttons & MK_LBUTTON) {
					quaternion	q(normalise(concat(dm.yx, 0, 512)));
					if (obj)
						object *= ~q;
					else
						camera.rot = camera.rot * q;
					Invalidate();

				} else if (buttons & MK_RBUTTON) {
					float	dist		= min(GetDist(camera.trans4.xyz - object.trans4.xyz), 1) / 100;
					float3	d			= concat(dm * float2{dist, -dist}, 0);
					if (obj)
						object *= translate(-d);
					else
						camera *= translate(camera * d);

					Invalidate();
				}
				mouse	= new_mouse;
				break;
			}

			case WM_MOUSEWHEEL: {
				bool		obj		= (wParam & MK_SHIFT) || GetShaderParameter("orientation");
				float		dist	= min(GetDist(camera.trans4.xyz - object.trans4.xyz), 1) / 512;
				float3		d{0, 0, (short)HIWORD(wParam) * dist};
				if (obj)
					object *= translate(-d);
				else
					camera *= translate(camera * d);
				Invalidate();
				break;
			}

			case WM_ISO_TIMER:
				Invalidate();
				break;

			case WM_NCDESTROY:
				delete this;
				break;
			default:
				return Super(message, wParam, lParam);
		}
		return 0;
	}

	ViewShader(MainWindow &_main, const WindowPos &wpos, const ISO_ptr<void> p);
};

Texture			noisemap;

ViewShader::ViewShader(MainWindow &main, const WindowPos &wpos, ISO_ptr<void> p) : main(main), shader(p) {
	Create(wpos, get_id(p), CHILD | VISIBLE, CLIENTEDGE);
	output = RenderOutputFinder::FindAndCreate((RenderWindow*)hWnd, GetClientRect().Size(), RenderOutput::DIMENSIONS_3);
//	output = RenderOutputFinder::FindAndCreate(this, GetClientRect().Size(), RenderOutput::DIMENSIONS_2);
	materialColour	= colour(1, 1, 1);
	Invalidate();
	Timer::Start(1 / 60.f);

	if (!noisemap) {
		(ISO_ptr<void>&)noisemap = ISO::root("data")["noisemap"];
		AddShaderParameter<crc32_const("noisemap")>(noisemap);
	}

}

void ViewShader::DrawScene(GraphicsContext &ctx, const float3x4 &eye, const RenderView &rv) {
	ctx.SetRenderTarget(rv.display);

	float3x4	object_mat	= inverse(float3x4(object));
	float4x4	camera_mat	= eye * inverse(perspective_projection_fov(rv.fov, .1f) * rv.offset);
	float		t			= time;
	/*
	AddShaderParameter("camera_mat",		camera_mat);
	AddShaderParameter("lightPos",			light);
	AddShaderParameter("object_mat",		object_mat);
	AddShaderParameter("time",				t);
	AddShaderParameter("materialColour",	materialColour);
	*/
	ShaderVal	m[] = {
		{"camera_mat",		&camera_mat},
		{"lightPos",		&light},
		{"object_mat",		&object_mat},
		{"time",			&t},
		{"materialColour",	&materialColour}
	};


	ctx.SetWindow(rv.window);
	PostEffects	post(ctx);
	Set(ctx, shader, ISO::MakeBrowser(ShaderVals(m)));
	post.DrawRect(float2(-one), float2(one));
}

Control MakeHTMLViewer(const WindowPos &wpos, const char *title, const char *text, size_t len);

struct Signature {
	struct Element {
		ComponentType	type;
		const char		*usage;
		int				usage_index;
		Element() {}
		Element(ComponentType _type, const char *_usage, int _usage_index = 0) : type(_type), usage(_usage), usage_index(_usage_index) {}
		bool	matches(const dx::SIG::Element &d, dx::SIG *sig) const {
			return strcmp(usage, d.name.get(sig)) == 0 && usage_index == d.semantic_index;
		}
	};

	int		num_inputs;
	int		num_outputs;
	Element	*elements;

	template<typename T> static Element	MakeElement(const char *usage, int usage_index = 0) {
		return Element(GetComponentType<T>(), usage, usage_index);
	}

	Signature(int _num_inputs, int _num_outputs, Element *_elements) : num_inputs(_num_inputs), num_outputs(_num_outputs), elements(_elements) {}
	bool	matches(dx::ISGN *isig, dx::OSGN *osig) const;
	bool	matches(const DX11Shader::SubShader &s) const;
};

bool Signature::matches(dx::ISGN *isig, dx::OSGN *osig) const {
	if (isig->num_elements == num_inputs && osig->num_elements == num_outputs) {
		Element	*e = elements;
		for (auto &i : isig->Elements()) {
			if (!e++->matches(i, isig))
				return false;
		}
		for (auto &i : osig->Elements()) {
			if (!e++->matches(i, osig))
				return false;
		}
		return true;
	}
	return false;
}

bool Signature::matches(const DX11Shader::SubShader &s) const {
	dx::DXBC *dxbc = (dx::DXBC*)s.raw();
	return matches(dxbc->GetBlob<dx::ISGN>(), dxbc->GetBlob<dx::OSGN>());
}

Signature::Element	sig[] = {
	Signature::MakeElement<float2>("POSITION"),
	Signature::MakeElement<float3>("SV_Position"),
	Signature::MakeElement<float3>("ORIGIN"),
	Signature::MakeElement<float3>("DIR"),
};

class EditorDX11Shader : public Editor {
	virtual bool Matches(const ISO::Browser &b) {
		if (b.GetTypeDef()->SameAs<DX11Shader>()) {
			DX11Shader	*shader = b;
			return shader->Has(SS_VERTEX) && Signature(1, 3, sig).matches(shader->sub[SS_VERTEX]);
		}
		return false;
	}
	virtual Control Create(MainWindow &main, const WindowPos &wpos, const ISO_VirtualTarget &v) {
		return *new ViewShader(main, wpos, v.GetPtr());
	}
} editordx11shader;
