#include "../includes.h"

void RenderCallback()
{
	Gui::BuildScene();
}

void BeginSceneCallback()
{
	Gui::DrawScene();
}

void ResetCallback()
{
	Gui::OnReset();
}

void WndProcHandlerCallback(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	Gui::WndProcHandler(hwnd, msg, wparam, lparam);
}

bool Window::BuildWindow(HINSTANCE hInstance, const char* pszWindowName, int iSizeX, int iSizeY, int iPositionX, int iPositionY)
{
	if (!DXWFInitialization(hInstance))
	{
		MessageBox(NULL, "DXWF Initilization error", "", MB_OK | MB_ICONERROR);
		return 1;
	}

	DXWFRendererCallbacks(DXWF_RENDERER_LOOP, RenderCallback);
	DXWFRendererCallbacks(DXWF_RENDERER_BEGIN_SCENE_LOOP, BeginSceneCallback);
	DXWFRendererCallbacks(DXWF_RENDERER_RESET, ResetCallback);
	DXWFWndProcCallbacks(DXWF_WNDPROC_WNDPROCHANDLER, WndProcHandlerCallback);
	DXWFSetDragTitlebarYSize(25);
	DXWFSetDragDeadZoneXOffset(28, 28);

	if (!DXWFCreateWindow(pszWindowName, iPositionX, iPositionY, iSizeX, iSizeY, WS_POPUP, WS_EX_TRANSPARENT, ENABLE_WINDOW_ALPHA | ENABLE_WINDOW_BLUR | ENABLE_POPUP_RESIZE, NULL))
	{
		MessageBox(NULL, "DXWF Create window error", "", MB_OK | MB_ICONERROR);
		return false;
	}

	if (!Gui::Initialize(DXWFGetHWND(), DXWFGetD3DDevice()))
	{
		MessageBox(NULL, "Gui initialization failed", "", MB_OK | MB_ICONERROR);
		return false;
	}

	DXWFRenderLoop();

	DXWFTerminate();

	return true;
}
