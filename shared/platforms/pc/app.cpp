#include "app.h"
#include "hook.h"
#include "utilities.h"
#include "gesture.h"
#include "shader.h"

using namespace iso;
using namespace win;

filename iso::UserDir() {
	return "";
}

point iso::GetSize(const RenderWindow *win) { return Control((HWND)win).GetClientRect().Size(); }

GestureList	GestureList::root, *GestureList::current_root;

void GestureList::Process(const Touch *touches, int num, Touch::PHASE phase) {
	for (iterator i = begin(); i != end(); ++i)
		i->Update(touches, num, phase);
}

Touch MakeTouch(param(float2) &p, Touch::PHASE phase) {
	Touch	t;
	t.phase	= phase;
	t.taps	= 1;
	t.pos	= p;
	return t;
}

LRESULT Application::Proc(UINT message, WPARAM wParam, LPARAM lParam) {
	switch (message) {
		case WM_SIZE:
			if (output)
				output->SetSize((RenderWindow*)hWnd, to_point(Point(lParam)));
			break;

		case WM_MOUSEWHEEL: {
			float	t = (short)HIWORD(wParam) / 256.f;
			Controller::_AddAnalog(CANA_TRIGGER, max(float2{-t, t}, zero));
			break;
		}

		case WM_LBUTTONDOWN:
			lmouse	= to_point(Point(lParam));
			Controller::_SetButton(CBUT_MOUSE_L);
			GestureList::root.Process(MakeTouch(NormalisedPos(lmouse), Touch::BEGAN), Touch::BEGAN);
			break;

		case WM_LBUTTONUP:
			Controller::_ClearButton(CBUT_MOUSE_L);
			break;

		case WM_RBUTTONDOWN:
			rmouse	= to_point(Point(lParam));
			Controller::_SetButton(CBUT_MOUSE_R);
			break;

		case WM_RBUTTONUP:
			Controller::_ClearButton(CBUT_MOUSE_R);
			break;

		case WM_MOUSEMOVE: {
			auto	newmouse	= to_point(Point(lParam));
			Controller::_SetAnalog(CANA_PTR, NormalisedPos(newmouse));

			if (wParam & MK_LBUTTON) {
				Controller::_AddAnalog(CANA_LEFT, (to_float2(newmouse) - to_float2(lmouse)) / 64.0f);
				lmouse	= newmouse;
			}
			if (wParam & MK_RBUTTON) {
				Controller::_AddAnalog(CANA_RIGHT, (to_float2(newmouse) - to_float2(rmouse)) / 64.0f);
				rmouse	= newmouse;
			}
			break;
		}

		case WM_KEYDOWN: {
			if (KeyboardKey *k = find(keys, int(wParam)))
				Controller::_SetButton(k->button);
			break;
		}

		case WM_KEYUP: {
			if (KeyboardKey *k = find(keys, int(wParam)))
				Controller::_ClearButton(k->button);
			break;
		}
/*
		case WM_PAINT: {
			PAINTSTRUCT	paint;
			BeginPaint(*this, &paint);
			if (ready) {
				BeginScene();
				Render();
				EndScene();
				Present();
			}
			EndPaint(*this, &paint);
			break;
		}
		*/
		case WM_ACTIVATEAPP:
			active = wParam == TRUE;
			ISO_TRACEF("active = ") << active << '\n';
			break;

		case WM_NCDESTROY:
			PostQuitMessage(0);
			//delete this;
			break;

		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

bool Application::SetOutput(const point &size, RenderOutput::Flags flags) {
	if (!output || any(size != DisplaySize()) || flags != GetOutputFlags()) {
		output = 0;
		if (RenderOutput *o = RenderOutputFinder::FindAndCreate((RenderWindow*)hWnd, size, flags))
			output = o;

		Move(GetRect().Centre(AdjustRect(win::Rect(0, 0, size.x, size.y), CAPTION | THICKFRAME, false).Size()));
	}
	return true;
}

void Application::Run() {
	while (win::ProcessMessage(false)) {
		try {
			PROFILE_CPU_EVENT("Update");
			AppEvent(AppEvent::UPDATE).send();

			PROFILE_CPU_EVENT_NEXT("Render");
			output->BeginFrame(ctx);
			AppEvent(AppEvent::RENDER).send();
			output->EndFrame(ctx);

			//if (!active)
			//	Sleep(50);
		} catch (const char *s) {
			MessageBoxA(*this, s, "IsoView", MB_OK | MB_ICONERROR);
		}
		PROFILER_END_FRAME();
	}
	exiting = true;
}

Application::Application(const char *title) : active(true), exiting(false) {
	AppEvent(AppEvent::PRE_GRAPHICS).send();
	Create(WindowPos(NULL, Rect(100,100,1024,1024)), title, OVERLAPPEDWINDOW);
	Show(SW_SHOW);
	Window::Update();
	SetOutput(to_point(GetClientRect().Size()), RenderOutput::NONE);
	ISO::iso_bin_allocator().set(&graphics);
}

int __stdcall WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,LPSTR lpCmdLine,int nCmdShow) {
//	auto	install = iso_winrt::Windows::ApplicationModel::Package::Current->InstalledLocation();
//	auto	dir		= install->Path();

	//ApplyHooks();
	return IsoMain(lpCmdLine);
}

