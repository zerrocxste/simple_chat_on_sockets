namespace Gui
{
	bool Initialize(HWND hWnd, LPDIRECT3DDEVICE9 pD3d9Device);
	void OnReset();
	void BeginScene();
	void EndScene();
	bool BuildNewDesktopStyleWindow(const char* pszTitle, bool* pBackButtonArg);
	void EndWindow();
	void BuildScene();
	void DrawScene();
	void WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	void SetTheme(bool IsWhite);
	extern ImFont* fTitle;
}