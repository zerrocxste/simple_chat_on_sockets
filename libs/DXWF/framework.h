//DirectX Window Framwork
//author: zerrocxste

#pragma once

enum DXWF_WNDPROC_CALLBACKS
{
	DXWF_WNDPROC_WNDPROCHANDLER,
	DXWF_WNDPROC_WM_SIZE,
	DXWF_WNDPROC_WM_MOUSEMOVE,
	DXWF_WNDPROC_WM_LBUTTONDOWN,
	DXWF_WNDPROC_WM_LBUTTONUP,
	DXWF_WNDPROC_WM_SYSKEYDOWN,
	DXWF_WNDPROC_WM_SYSCHAR,
	DXWF_WNDPROC_WM_SYSKEYUP,
	DXWF_WNDPROC_WM_KEYDOWN,
	DXWF_WNDPROC_WM_KEYUP,
	DXWF_WNDPROC_WM_CHAR,
	DXWF_WNDPROC_WM_PAINT,
	DXWF_WNDPROC_CALLBACKS_MAX_SIZE
};

enum DXWF_RENDER_CALLBACKS
{
	DXWF_RENDERER_LOOP,
	DXWF_RENDERER_BEGIN_SCENE_LOOP,
	DXWF_RENDERER_RESET,
	DXWF_RENDER_CALLBACKS_MAX_SIZE
};

enum USER_DXWF_FLAGS
{
	NONE = 0,
	ENABLE_WINDOW_ALPHA = 1 << 0,
	ENABLE_WINDOW_BLUR = 1 << 1,
	ENABLE_BLUR_ACRYLIC = 1 << 2,
	ENABLE_BLUR_SYSTEM_COLORIZATION = 1 << 3,
	ENABLE_POPUP_RESIZE = 1 << 4
};

struct dxwf_color
{
	dxwf_color() { r = g = b = 255; a = 1; };
	dxwf_color(int R, int G, int B, int A) : r(R), g(G), b(B), a(A) {};
	int r, g, b, a;
};

typedef void (*callback_wndproc)(HWND, UINT, WPARAM, LPARAM);
typedef void (*callback)();

BOOL DXWFInitialization(
	HINSTANCE hInstance);

void DXWFWndProcCallbacks(
	DXWF_WNDPROC_CALLBACKS WndProcCallbackNum,
	callback_wndproc a);

void DXWFRendererCallbacks(
	DXWF_RENDER_CALLBACKS RenderCallbackNum,
	callback cCallbackFunction);

BOOL DXWFCreateWindow(
	LPCSTR szWindowName,
	const int iWindowPositionX, const int iWindowPositionY,
	const int iWindowSizeX, const int iWindowSizeY,
	DWORD dwWindowArg,
	DWORD dwExStyle,
	DWORD dx_window_flags,
	int pIcon);

HWND DXWFGetHWND();

LPDIRECT3DDEVICE9 DXWFGetD3DDevice();

void DXWFSetWindowVisibleState(bool show);
void DXWFSetWindowHideState(bool show);

void DXWFSetWindowPos(int x, int y);
void DXWFSetWindowSize(int x, int y);

void DXWFEnableTransparentWindow();
void DXWFDisableTransparentWindow();

void DXWFEnableBlur(DWORD blur_flags, dxwf_color blur_color);
void DXWFDisableBlur();
void DXWFSetBlurColor(dxwf_color blur_color);

void DXWFSetDragTitlebarYSize(int YSize);
void DXWFSetDragDeadZoneXOffset(int iTopLeftDeadZone, int iTopRightDeadZone);

void DXWFSetFramerateLimit(
	const DWORD iMaxFPs);

void DXWFRenderLoop();

void DXWFTerminate();


