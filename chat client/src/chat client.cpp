#include "includes.h"

namespace console
{
	FILE* out;
	void attach(const char* title)
	{
		AllocConsole();
		freopen_s(&out, "conout$", "w", stdout);
		MoveWindow(GetConsoleWindow(), 0, 0, 400, 300, FALSE);
	}
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevHistance, LPSTR pszCmd, int cmd)
{
#ifdef _USERDEBUG
	console::attach("debug");
#endif

	LoadLibrary("dwmapi.dll");

	const int iWindowSize[2] = { 450, 350 };
	const int iWindowPositon[2] = { 
		(GetSystemMetrics(SM_CXSCREEN) / 2 ) - (iWindowSize[0] / 2) , 
		(GetSystemMetrics(SM_CYSCREEN) / 2) - (iWindowSize[1] / 2) 
	};

	if (!Window::BuildWindow(hInstance, pszApplicationName, iWindowSize[0], iWindowSize[1], iWindowPositon[0], iWindowPositon[1]))
		MessageBox(NULL, "Failed build window", pszApplicationName, MB_OK | MB_ICONERROR);

	printf("[+] Application terminates\n");

	return 0;
}
