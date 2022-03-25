#include "app.h"
#include "utilities.h"
#include "shader.h"
#include "postprocess/post.h"

using namespace iso;
using namespace win;

#include "OVR_Version.h"

#define OVR_VER	((OVR_MAJOR_VERSION << 8) | OVR_MINOR_VERSION)
#define OVR_D3D_VERSION 11

#include "OVR_CAPI.h"
#include "OVR_CAPI_D3D.h"

//#pragma comment(lib, "LibOVR")

#ifdef USE_DX11
typedef ID3D11Texture2D	D3DTexture;
#else
typedef ID3D12Resource	D3DTexture;
#endif

struct OculusOutput : RenderOutput {
	Graphics::Display	display;
	ovrSession			hmd;
	ovrTrackingState	ts;
	uint32				frame;
//	ovrVector3f			hmdToEyeViewOffset[2];
	ovrPosef			hmdToEyePose[2];
	ovrHmdDesc			hmdDesc;
	ovrTextureSwapChain	swap_chain;
	ovrMirrorTexture	mirror_tex;
	ovrLayerEyeFov		layer;

	OculusOutput(ovrSession hmd, RenderWindow *window, MSAA msaa) : hmd(hmd), frame(0) {
		ovrResult	result;

		hmdDesc = ovr_GetHmdDesc(hmd);

		ovr_SetTrackingOriginType(hmd, ovrTrackingOrigin_FloorLevel);
		ovr_RecenterTrackingOrigin(hmd);

		ovrSizei	size[2] = {
			ovr_GetFovTextureSize(hmd, ovrEye_Left,  hmdDesc.DefaultEyeFov[0], 1.0f),
			ovr_GetFovTextureSize(hmd, ovrEye_Right, hmdDesc.DefaultEyeFov[1], 1.0f)
		};
		ovrVector2i	pos[2]	= {
			{0,			0},
			{size[0].w,	0}
		};

		ovrTextureSwapChainDesc	swap_desc;
		swap_desc.Type			= ovrTexture_2D;
		swap_desc.Format		= OVR_FORMAT_R8G8B8A8_UNORM;
		swap_desc.ArraySize		= 1;
		swap_desc.Width			= size[0].w + size[1].w;
		swap_desc.Height		= max(size[0].h, size[1].h);
		swap_desc.MipLevels		= 1;
		swap_desc.SampleCount	= 1;
		swap_desc.StaticImage	= false;
		swap_desc.MiscFlags		= 0;
		swap_desc.BindFlags		= ovrTextureBind_DX_RenderTarget;
		result = ovr_CreateTextureSwapChainDX(hmd, graphics.Device(), &swap_desc, &swap_chain);

		// Initialize our single full screen Fov layer
		layer.Header.Type		= ovrLayerType_EyeFov;
		layer.Header.Flags		= 0;
		for (int i = 0; i < 2; i++) {
			ovrEyeRenderDesc desc	= ovr_GetRenderDesc(hmd, (ovrEyeType)i, hmdDesc.DefaultEyeFov[i]);
			//hmdToEyeViewOffset[i]	= desc.HmdToEyeOffset;
			hmdToEyePose[i]			= desc.HmdToEyePose;
			layer.ColorTexture[i]	= swap_chain;
			layer.Fov[i]			= desc.Fov;
			layer.Viewport[i].Pos	= pos[i];
			layer.Viewport[i].Size	= size[i];
			(float4p&)layer.RenderPose[i].Orientation	= quaternion(identity).v;
			(float3p&)layer.RenderPose[i].Position		= float3(zero);
		}

		// Create a mirror to see on the monitor
		ovrMirrorTextureDesc	mirror_desc;
		mirror_desc.Format		= OVR_FORMAT_R8G8B8A8_UNORM;
		mirror_desc.Width		= hmdDesc.Resolution.w;
		mirror_desc.Height		= hmdDesc.Resolution.h;
		mirror_desc.MiscFlags	= 0;
		mirror_tex				= 0;
		result = ovr_CreateMirrorTextureDX(hmd, graphics.Device(), &mirror_desc, &mirror_tex);

		display.SetSize(window, point{hmdDesc.Resolution.w, hmdDesc.Resolution.h});
	}

	~OculusOutput() {
		ovr_DestroyTextureSwapChain(hmd, swap_chain);
		ovr_DestroyMirrorTexture(hmd, mirror_tex);
		ovr_Destroy(hmd);
	}

	Flags	GetFlags() override	{
		return RenderOutput::DIMENSIONS_3;
	}
	void	BeginFrame(GraphicsContext &ctx) override {
		ovr_CommitTextureSwapChain(hmd, swap_chain);
		graphics.BeginScene(ctx);
	}
	void	EndFrame(GraphicsContext &ctx) override {
		// Submit frame with one layer we have
		ovrLayerHeader* layers[]	= {&layer.Header};
		ovrResult		result		= ovr_SubmitFrame(hmd, 0, nullptr, layers, uint32(num_elements(layers)));

		com_ptr<D3DTexture>	d3dtex;
		if (OVR_SUCCESS(ovr_GetMirrorTextureBufferDX(hmd, mirror_tex, d3dtex.uuid(), (void**)&d3dtex))) {
			ctx.Blit(display.GetDispSurface(), d3dtex.get());
			display.Present();
		}
		graphics.EndScene(ctx);

		Update();
	}
	void	Update() {
		++frame;
		double time = ovr_GetPredictedDisplayTime(hmd, frame);
		ts = ovr_GetTrackingState(hmd, time, true);
		ovr_CalcEyePoses(ts.HeadPose.ThePose, hmdToEyePose, layer.RenderPose);

		_Controller::_SetAnalog(CANA_LEFT_POS,	load<3>(&ts.HandPoses[0].ThePose.Position.x));
		_Controller::_SetAnalog(CANA_RIGHT_POS, load<3>(&ts.HandPoses[1].ThePose.Position.x));

		ovrInputState	inputState;
		if (OVR_SUCCESS(ovr_GetInputState(hmd, ovrControllerType_Touch, &inputState))) {
			_Controller::_SetButton(ControllerButton(inputState.Buttons));
			_Controller::_SetAnalog(CANA_TRIGGER_L, inputState.IndexTrigger[0]);//trigger
			_Controller::_SetAnalog(CANA_TRIGGER_R, inputState.IndexTrigger[1]);

			_Controller::_SetAnalog(CANA_TRIGGER_L, inputState.HandTrigger[0]);//grip
			_Controller::_SetAnalog(CANA_TRIGGER_R, inputState.HandTrigger[1]);

			_Controller::_SetAnalog(CANA_LEFT,	load<2>(&inputState.Thumbstick[0].x));
			_Controller::_SetAnalog(CANA_RIGHT,	load<2>(&inputState.Thumbstick[1].x));
		}
	}
	RenderView	GetView(int i) override {
		const ovrRecti			&r = layer.Viewport[i];
		const ovrPosef			&p = layer.RenderPose[i];
		const ovrFovPort		&f = layer.Fov[i];

		RenderView	v;
		v.offset	= quaternion(p.Orientation.x, -p.Orientation.y, -p.Orientation.z, p.Orientation.w) * translate(-p.Position.x, p.Position.y, p.Position.z);
		v.fov		= float4{f.LeftTan, f.UpTan, f.RightTan, f.DownTan};
		v.window	= rect::with_length((point&)r.Pos, (point&)r.Size);

		int			index;
		com_ptr<D3DTexture>	d3dtex;
		ovr_GetTextureSwapChainCurrentIndex(hmd, swap_chain, &index);
		ovr_GetTextureSwapChainBufferDX(hmd, swap_chain, index, d3dtex.uuid(), (void**)&d3dtex);
		v.display	= d3dtex.get();

		return v;
	}
	virtual_container<RenderView>	Views() override {
		return transformc(int_range(2), [this](int i) { return GetView(i); });
	}
	point	DisplaySize() override {
		return {hmdDesc.Resolution.w, hmdDesc.Resolution.h};
	}
	void	SetSize(RenderWindow *window, const point &size) override {
	}
};

struct Oculus : RenderOutputFinderPri<Oculus, 1>, Handles2<Oculus, AppEvent> {
	void	operator()(AppEvent *ev) {
		switch (ev->state) {
			case AppEvent::PRE_GRAPHICS:
#ifdef _DEBUG
				ovrInitParams	params;
				clear(params);
				params.Flags = ovrInit_Debug;
				ovr_Initialize(&params);
#else
				ovr_Initialize(NULL);
#endif
				break;

			case AppEvent::END: {
				ovr_Shutdown();
				break;
			}
		}
	}

	RenderOutput::Flags	Capability(RenderOutput::Flags flags) {
		return (flags & RenderOutput::DIMENSIONS_3) && ovr_GetHmdDesc(0).Type != ovrHmd_None ? RenderOutput::DIMENSIONS_3 : RenderOutput::NONE;
	}

	RenderOutput*		Create(RenderWindow *window, const point &size, RenderOutput::Flags flags) {
		if (flags & RenderOutput::DIMENSIONS_3) {
			ovrSession		hmd;
			ovrGraphicsLuid luid;
			if (ovr_Create(&hmd, &luid) == ovrSuccess)
				return new OculusOutput(hmd, window, MSAA((flags & RenderOutput::_MSAA_MASK) / 16));
		}
		return 0;
	}
} oculus;

extern "C" void *include_oculus() { return &oculus; }
