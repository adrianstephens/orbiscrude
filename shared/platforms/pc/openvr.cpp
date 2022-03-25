#include "app.h"
#include "utilities.h"
#include "shader.h"
#include "postprocess/post.h"
#include "base/algorithm.h"
#include "render.h"
#include "object.h"
#include "dx/dxgi_helpers.h"

#define OPENVR_INTERFACE_INTERNAL
#include "openvr.h"

//#pragma comment(lib, "openvr_api")

using namespace iso;

namespace iso {

class OpenVRSystems {
	template<typename I, const char *const *version> struct system {
		I	*p;
		I*	get(class OpenVRContext *ctx) {
			return p = (I*)ctx->GetInterface(p, *version);
		}
	};
public:
	system<vr::IVRSystem,			&vr::IVRSystem_Version>				System;
	system<vr::IVRChaperone,		&vr::IVRChaperone_Version>			Chaperone;
	system<vr::IVRChaperoneSetup,	&vr::IVRChaperoneSetup_Version>		ChaperoneSetup;
	system<vr::IVRCompositor,		&vr::IVRCompositor_Version>			Compositor;
	system<vr::IVRHeadsetView,		&vr::IVRHeadsetView_Version>		HeadsetView;
	system<vr::IVROverlay,			&vr::IVROverlay_Version>			Overlay;
	system<vr::IVROverlayView,		&vr::IVROverlayView_Version>		OverlayView;
	system<vr::IVRResources,		&vr::IVRResources_Version>			Resources;
	system<vr::IVRRenderModels,		&vr::IVRRenderModels_Version>		RenderModels;
	system<vr::IVRExtendedDisplay,	&vr::IVRExtendedDisplay_Version>	ExtendedDisplay;
	system<vr::IVRSettings,			&vr::IVRSettings_Version>			Settings;
	system<vr::IVRApplications,		&vr::IVRApplications_Version>		Applications;
	system<vr::IVRTrackedCamera,	&vr::IVRTrackedCamera_Version>		TrackedCamera;
	system<vr::IVRScreenshots,		&vr::IVRScreenshots_Version>		Screenshots;
	system<vr::IVRDriverManager,	&vr::IVRDriverManager_Version>		DriverManager;
	system<vr::IVRInput,			&vr::IVRInput_Version>				Input;
	system<vr::IVRIOBuffer,			&vr::IVRIOBuffer_Version>			IOBuffer;
	system<vr::IVRSpatialAnchors,	&vr::IVRSpatialAnchors_Version>		SpatialAnchors;
	system<vr::IVRDebug,			&vr::IVRDebug_Version>				Debug;
	system<vr::IVRNotifications,	&vr::IVRNotifications_Version>		Notifications;
};

class OpenVRContext : public OpenVRSystems {
	dll_function<uint32(vr::EVRInitError*, vr::EVRApplicationType, const char*)>	VR_InitInternal2;
	dll_function<void()>															VR_ShutdownInternal;
	dll_function<bool()>															VR_IsHmdPresent;
	dll_function<bool()>															VR_IsRuntimeInstalled;
	dll_function<bool(char*, uint32, uint32*)>										VR_GetRuntimePath;
	dll_function<const char*(vr::EVRInitError)>										VR_GetVRInitErrorAsSymbol;
	dll_function<const char*(vr::EVRInitError)>										VR_GetVRInitErrorAsEnglishDescription;
	dll_function<void*(const char*, vr::EVRInitError*)>								VR_GetGenericInterface;
	dll_function<bool(const char*)>													VR_IsInterfaceVersionValid;
	dll_function<uint32()>															VR_GetInitToken;

	uint32	token;

	bool init() {
		if (!VR_InitInternal2) {
			auto	lib = load_library("openvr_api", 0, "D:\\dev\\sdk\\openvr\\bin\\win64");
			if (!lib)
				return false;
			VR_InitInternal2.bind(lib, "VR_InitInternal2");
			VR_IsHmdPresent.bind(lib, "VR_IsHmdPresent");
			VR_IsRuntimeInstalled.bind(lib,						"VR_IsRuntimeInstalled");
			VR_GetRuntimePath.bind(lib,							"VR_GetRuntimePath");
			VR_GetVRInitErrorAsSymbol.bind(lib,					"VR_GetVRInitErrorAsSymbol");
			VR_GetVRInitErrorAsEnglishDescription.bind(lib,		"VR_GetVRInitErrorAsEnglishDescription");
			VR_GetGenericInterface.bind(lib,					"VR_GetGenericInterface");
			VR_GetInitToken.bind(lib,							"VR_GetInitToken");
			VR_IsInterfaceVersionValid.bind(lib,				"VR_IsInterfaceVersionValid");
		}
		return true;
	}

public:
	void*	GetInterface(void *p, const char *version) {
		if (token != VR_GetInitToken()) {
			clear(*(OpenVRSystems*)this);
			token	= VR_GetInitToken();
			p		= nullptr;
		}
		
		if (!p) {
			vr::EVRInitError error;
			return VR_GetGenericInterface(version, &error);
		}
		return p;
	}

	bool	IsHmdPresent() {
		return init() && VR_IsHmdPresent();
	}
	bool	IsRuntimeInstalled() {
		return init() && VR_IsRuntimeInstalled();
	}
	string	RuntimePath() {
		if (init()) {
			uint32	size;
			if (VR_GetRuntimePath(nullptr, 0, &size)) {
				string	s(size);
				VR_GetRuntimePath(s, size, &size);
				return s;
			}
		}
		return "";
	}
	const char *AsSymbol(vr::EVRInitError error) {
		return init() ? VR_GetVRInitErrorAsSymbol(error) : nullptr;
	}
	const char* AsEnglishDescription(vr::EVRInitError error) {
		return init() ? VR_GetVRInitErrorAsEnglishDescription(error) : nullptr;
	}

	vr::IVRSystem* GetSystem(vr::EVRInitError& error, vr::EVRApplicationType eApplicationType, const char* pStartupInfo = nullptr) {
		token	= VR_InitInternal2(&error, eApplicationType, pStartupInfo);

		if (error == vr::VRInitError_None) {
			if (VR_IsInterfaceVersionValid(vr::IVRSystem_Version))
				return System.get(this);
		
			VR_ShutdownInternal();
			error = vr::VRInitError_Init_InterfaceNotFound;
		}

		return nullptr;
	}
	void	Shutdown() {
		if (VR_ShutdownInternal)
			VR_ShutdownInternal();
	}
} VR;

template<> struct _ComponentType<vr::HmdVector3_t> : _ComponentType<float[3]> {};

template<> VertexElements GetVE<vr::RenderModel_Vertex_t>() {
	static VertexElement ve[] = {
		VertexElement(&vr::RenderModel_Vertex_t::vPosition,			"position"_usage),
		VertexElement(&vr::RenderModel_Vertex_t::vNormal,			"normal"_usage),
		VertexElement(&vr::RenderModel_Vertex_t::rfTextureCoord,	"texcoord"_usage)
	};
	return ve;
};
}

struct OpenVRModel {
	string	name;

	VertexBuffer<vr::RenderModel_Vertex_t>	vb;
	IndexBuffer<uint16>		ib;
	Texture					tex;
	cuboid					box;
	uint32					num_verts, num_prims;

	OpenVRModel(const char *_name, vr::RenderModel_t *model, vr::RenderModel_TextureMap_t *diffuse) : name(_name), box(empty) {
		num_verts	= model->unVertexCount;
		num_prims	= model->unTriangleCount;
		vb.Init(model->rVertexData, model->unVertexCount);
		ib.Init(model->rIndexData, model->unTriangleCount * 3);
		if (diffuse)
			tex.Init(TEXF_R8G8B8A8, diffuse->unWidth, diffuse->unHeight, 1, 1, MEM_DEFAULT, (void*)diffuse->rubTextureMapData, diffuse->unWidth * 4);

		for (const vr::RenderModel_Vertex_t *p = model->rVertexData, *e = p + model->unVertexCount; p != e; ++p)
			box |= position3(load<float3>(p->vPosition.v));
	}
	void	Draw(GraphicsContext &ctx) {
		static ISO_ptr<pass> specular	= *ISO::root("data")["default"]["tex_specular"][0];

		ShaderVal	m[] = {
			{"diffuse_samp",	&tex		},
//			{"glossiness",		10			},
		};

		Set(ctx, specular, ISO::MakeBrowser(ShaderVals(m)));
		ctx.SetVertices(vb);
		ctx.SetIndices(ib);
		ctx.DrawIndexedPrimitive(PRIM_TRILIST, 0, num_verts, 0, num_prims);
	}

};

OpenVRModel *LoadRenderModel(const char *name) {
	vr::IVRRenderModels	*rm		= VR.RenderModels.get(&VR);
	vr::RenderModel_t	*model	= NULL;
	OpenVRModel			*model2	= 0;

	if (rm->LoadRenderModel_Async(name, &model) == vr::VRRenderModelError_None && model) {
		vr::RenderModel_TextureMap_t	*diffuse = NULL;
//		if (model->diffuseTextureId) {
			if (rm->LoadTexture_Async(model->diffuseTextureId, &diffuse) == vr::VRRenderModelError_Loading)
				return 0;
//		}
		model2 = new OpenVRModel(name, model, diffuse);
		rm->FreeTexture(diffuse);
		rm->FreeRenderModel(model);
	}
	return model2;
}

string GetTrackedDeviceString(vr::IVRSystem *system, vr::TrackedDeviceIndex_t device, vr::ETrackedDeviceProperty prop) {
	uint32	len = system->GetStringTrackedDeviceProperty(device, prop, NULL, 0);
	if (len == 0)
		return "";

	char	*buffer = alloc_auto(char, len);
	system->GetStringTrackedDeviceProperty(device, prop, buffer, len);
	return buffer;
}

struct OpenVROutput : RenderOutput {
	Graphics::Display			display;
	uint32						width, height;

	struct EyeInfo {;
		float4					fov;
		float3x4				mat;
		Texture					tex;
	} eye[2];

	float3x4					head;

	vr::IVRSystem				*system;

	vr::TrackedDevicePose_t poses[vr::k_unMaxTrackedDeviceCount];
	uint64					buttons[vr::k_unMaxTrackedDeviceCount];
	uint8					types[vr::k_unMaxTrackedDeviceCount];
	OpenVRModel				*models[vr::k_unMaxTrackedDeviceCount];

	VertexBuffer<float2p>	hidden_mesh[2];
	uint32					hidden_count[2];

	static float3x4 ConvertMatrix(const vr::HmdMatrix34_t &mat) {
		return float3x4(
			float3{mat.m[0][0], -mat.m[1][0], -mat.m[2][0]},
			float3{-mat.m[0][1], mat.m[1][1], mat.m[2][1]},
			float3{-mat.m[0][2], mat.m[1][2], mat.m[2][2]},
			float3{mat.m[0][3], -mat.m[1][3], -mat.m[2][3]}
		);
	}

	float3x4	GetWorldMat(int device) {
		return  ConvertMatrix(poses[device].mDeviceToAbsoluteTracking) * rotate_in_x(pi);
	}

	void ProcessVREvent(const vr::VREvent_t &event) {
		switch (event.eventType) {
			case vr::VREvent_TrackedDeviceActivated:
				//SetupRenderModelForTrackedDevice(event.trackedDeviceIndex);
				//dprintf("Device %u attached. Setting up render model.\n", event.trackedDeviceIndex);
				break;
			case vr::VREvent_TrackedDeviceDeactivated:
				//dprintf("Device %u detached.\n", event.trackedDeviceIndex);
				break;
			case vr::VREvent_TrackedDeviceUpdated:
				//dprintf("Device %u updated.\n", event.trackedDeviceIndex);
				break;
		}
	}

	OpenVROutput(vr::IVRSystem *system, RenderWindow *window, MSAA msaa) : head(identity), system(system) {
		clear(buttons);
		clear(poses);
		clear(types);
		clear(models);

		// Query OpenVR for the output adapter index
		int	adapter_index = 0;
		system->GetDXGIOutputInfo(&adapter_index);
		graphics.Init(GetAdapter(adapter_index));

		system->GetRecommendedRenderTargetSize(&width, &height);
		SetSize(window, GetSize(window));

		for (int i = 0; i < 2; i++) {
			vr::HiddenAreaMesh_t hidden = system->GetHiddenAreaMesh((vr::EVREye)i);
			if (uint32 n = hidden.unTriangleCount * 3) {
				hidden_count[i] = n;
				float2p	*temp	= alloc_auto(float2p, n);
				for_each2n(hidden.pVertexData, temp, n, [](const vr::HmdVector2_t &a, float2p &b) { b = float2{a.v[0] * 2 - 1, a.v[1] * 2  - 1};} );
				hidden_mesh[i].Init(temp, n);
			}

			float	*f		= (float*)&eye[i].fov;
			system->GetProjectionRaw((vr::EVREye)i,	f + 0, f + 2, f + 1, f + 3);
			eye[i].fov		= abs(eye[i].fov);
			eye[i].mat		= ConvertMatrix(system->GetEyeToHeadTransform((vr::EVREye)i));
			eye[i].tex.Init(TEXF_A8B8G8R8, width, height, 1, 1, MEM_TARGET);
		}

		World::Global()->AddHandler<RenderEvent>(this);
	}

	~OpenVROutput() {
	}

	Flags	GetFlags()	{ return RenderOutput::DIMENSIONS_3; }

	void	BeginFrame(GraphicsContext &ctx) {
		graphics.BeginScene(ctx);
		if (poses[vr::k_unTrackedDeviceIndex_Hmd].bPoseIsValid)
			head = ConvertMatrix(poses[vr::k_unTrackedDeviceIndex_Hmd].mDeviceToAbsoluteTracking);
	}

	void	EndFrame(GraphicsContext &ctx) {
		auto compositor = VR.Compositor.get(&VR);
		if (compositor) {
#ifdef USE_DX12
			ctx.Transition(eye[0].tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			ctx.Transition(eye[1].tex, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
			ctx.FlushTransitions();
			vr::D3D12TextureData_t tex12[2] = {
				{*eye[0].tex, graphics.queue, 0 },
				{*eye[1].tex, graphics.queue, 0 },
			};
			vr::Texture_t eyes[2]	= {
				{tex12 + 0, vr::TextureType_DirectX12, vr::ColorSpace_Gamma},
				{tex12 + 1, vr::TextureType_DirectX12, vr::ColorSpace_Gamma}
			};
#else
			vr::Texture_t eyes[2]	= {
				{eye[0].tex.GetSurface(), vr::TextureType_DirectX, vr::ColorSpace_Gamma},
				{eye[1].tex.GetSurface(), vr::TextureType_DirectX, vr::ColorSpace_Gamma}
			};
#endif
			vr::VRTextureBounds_t bounds[2] = {
				{0, 0, 1, 1},
				{0, 0, 1, 1},
			};
			compositor->Submit(vr::Eye_Left,	eyes + 0, bounds + 0);
			compositor->Submit(vr::Eye_Right,	eyes + 1, bounds + 1);
		}
		ctx.Blit(display.GetDispSurface(), eye[0].tex, zero, rect::with_centre(point{width, height} / two, display.Size() / two));

		//PostEffects(ctx).
		display.MakePresentable(ctx);
		graphics.EndScene(ctx);
		display.Present();

		if (compositor) {
			//compositor->PostPresentHandoff();
			compositor->WaitGetPoses(poses, vr::k_unMaxTrackedDeviceCount, NULL, 0);
		}

		// Process OpenR events
		vr::VREvent_t event;
		while (system->PollNextEvent(&event, sizeof(event)))
			ProcessVREvent(event);

		// Process SteamVR controller state
		for (vr::TrackedDeviceIndex_t device = 0; device < vr::k_unMaxTrackedDeviceCount; device++) {
			vr::VRControllerState_t state;
			if (system->GetControllerState(device, &state, sizeof(state)))
				buttons[device] = state.ulButtonPressed;
		}
	}

	RenderView	GetView(int i) {
		RenderView	v;
		v.offset	= inverse(head * eye[i].mat);
		v.fov		= eye[i].fov;
		v.window	= rect(zero, point{width, height});
		v.display	= eye[i].tex;
		v.id		= i;
		return v;
	}
	virtual_container<RenderView> Views() {
		return transformc(int_range(2), [this](int i) { return GetView(i); });
	}
	void	DrawHidden(GraphicsContext &ctx, const RenderView &v) {
		ctx.SetVertices(hidden_mesh[v.id]);
		ctx.DrawVertices(PRIM_TRILIST, 0, hidden_count[v.id]);
	}

	point	DisplaySize()		{ return {width, height}; }

	void	SetSize(RenderWindow *window, const point &size) {
		if (size.x && size.y) {
			int		w		= width * 3 / 4;
			int		h		= height / 2;
			point	size2	= size.x * h > size.y * w
				? point{w, size.y * w / size.x}
				: point{size.x * h / size.y, h};
			display.SetSize(window, size2);
		}
	}

	void	operator()(RenderEvent &re) {
		float3x4	cam		= float3x4((float4x4)(re.consts.iview * inverse(head)));

		for (vr::TrackedDeviceIndex_t device = 1; device < vr::k_unMaxTrackedDeviceCount; device++) {
			if (system->IsTrackedDeviceConnected(device) && poses[device].bPoseIsValid) {
				OpenVRModel	*model = models[device];
				if (!model) {
					string		name	= GetTrackedDeviceString(system, device, vr::Prop_RenderModelName_String);
					OpenVRModel	**p		= find_if(models, [&name](OpenVRModel *model) { return model && model->name == name; });
					model				= p == end(models) ? LoadRenderModel(name) : *p;
					if (!model)
						continue;
					models[device]		= model;
				}

				float3x4	world	= cam * GetWorldMat(device);
				float4x4	worldViewProj(re.consts.viewProj * world);

				if (is_visible(model->box, worldViewProj)) {
					float	d	= (re.consts.view * position3(model->box.centre() + world.w)).v.z;
					re.AddRenderItem(this, MakeKey(RS_OPAQUE, d), device);
				}
			}
		}
	}

	void operator()(RenderEvent *re, uint32 extra) {
		float3x4	view	= inverse(head * re->offset);
		float3x4	cam		= re->consts.iview * view;
		re->consts.SetWorld(cam * GetWorldMat(extra));
		models[extra]->Draw(re->ctx);
	}
};

struct OpenVR : RenderOutputFinderPri<OpenVR, 0> {
	vr::IVRSystem	*system;

	RenderOutput::Flags	Capability(RenderOutput::Flags flags) {
		return (flags & RenderOutput::DIMENSIONS_3) && VR.IsHmdPresent() ? RenderOutput::DIMENSIONS_3 : RenderOutput::NONE;
	}

	RenderOutput	*Create(RenderWindow *window, const point &size, RenderOutput::Flags flags) {
		if ((flags & RenderOutput::DIMENSIONS_3) && VR.IsHmdPresent()) {
			if (!system) {
				vr::EVRInitError error = vr::VRInitError_None;
				system	= VR.GetSystem(error, vr::VRApplication_Scene);
			}
			if (system) {
				if (GetTrackedDeviceString(system, vr::k_unTrackedDeviceIndex_Hmd, vr::Prop_TrackingSystemName_String) != "oculus")
					return new OpenVROutput(system, window, MSAA((flags & RenderOutput::_MSAA_MASK) / 16));
			}
		}
		return 0;
	}
	OpenVR() : system(0) {}
	~OpenVR() {
		if (system)
			VR.Shutdown();
	}

} openvr;

extern "C" void *include_openvr() { return &openvr; }
