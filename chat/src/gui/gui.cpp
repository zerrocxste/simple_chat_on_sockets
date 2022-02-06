#include "../includes.h"

#include "../../libs/ImGui/imgui_fonts.h"

LRESULT ImGui_ImplWin32_WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

ImFont* Gui::fTitle;

bool Gui::Initialize(HWND hWnd, LPDIRECT3DDEVICE9 pD3d9Device)
{
	ImGui::CreateContext();
	ImGui::StyleColorsClassic();

	ImGui::GetIO().IniFilename = NULL;

	auto& style = ImGui::GetStyle();

	style.FrameRounding = 3.f;
	style.ChildRounding = 3.f;
	style.ChildBorderSize = 1.f;
	style.ScrollbarSize = 0.6f;
	style.ScrollbarRounding = 3.f;
	style.GrabRounding = 3.f;
	style.WindowRounding = 0.f;

	SetTheme(false);

	auto& io = ImGui::GetIO();

	io.Fonts->AddFontFromMemoryTTF(Montserrat_Light, IM_ARRAYSIZE(Montserrat_Light), 18.9f, NULL, ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());
	fTitle = io.Fonts->AddFontFromMemoryTTF(Stolzl_Light, IM_ARRAYSIZE(Stolzl_Light), 30.f, NULL, ImGui::GetIO().Fonts->GetGlyphRangesCyrillic());

	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX9_Init(pD3d9Device);
	ImGui_ImplDX9_CreateDeviceObjects();

	return true;
}

void Gui::OnReset()
{
	ImGui_ImplDX9_InvalidateDeviceObjects();
}

void Gui::BeginScene()
{
	ImGui_ImplWin32_NewFrame();
	ImGui_ImplDX9_NewFrame();

	ImGui::NewFrame();
}

void Gui::EndScene()
{
	ImGui::EndFrame();
	ImGui::Render();
}

bool Gui::BuildNewDesktopStyleWindow(const char* pszTitle, bool* pBackButtonArg)
{
	static bool bOpened = true;

	ImGui::SetNextWindowPos(ImVec2(), ImGuiCond_Always);

	ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize, ImGuiCond_Always);

	auto bBeginStatus = ImGui::Begin(
		pszTitle,
		&bOpened,
		ImGuiWindowFlags_::ImGuiWindowFlags_NoResize | ImGuiWindowFlags_::ImGuiWindowFlags_NoMove | ImGuiWindowFlags_::ImGuiWindowFlags_NoCollapse,
		pBackButtonArg);

	if (!bOpened)
		PostQuitMessage(0);

	return bBeginStatus;
}

void Gui::EndWindow()
{
	ImGui::End();
}

void Gui::BuildScene()
{
	BeginScene();

	static bool baBackButton[2];

	if (BuildNewDesktopStyleWindow(pszApplicationName, baBackButton))
	{
		Chat::Run(baBackButton);
		EndWindow();
	}

	EndScene();
}

void Gui::DrawScene()
{
	ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
}

void Gui::WndProcHandler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam);
}

void Gui::SetTheme(bool IsWhite)
{
	auto& style = ImGui::GetStyle();
	if (IsWhite)
	{
		style.Colors[ImGuiCol_FrameBg] = ImColor(200, 200, 200);
		style.Colors[ImGuiCol_FrameBgHovered] = ImColor(220, 220, 220);
		style.Colors[ImGuiCol_FrameBgActive] = ImColor(230, 230, 230);
		style.Colors[ImGuiCol_Separator] = ImColor(180, 180, 180);
		style.Colors[ImGuiCol_CheckMark] = ImColor(255, 172, 19);
		style.Colors[ImGuiCol_SliderGrab] = ImColor(255, 172, 19);
		style.Colors[ImGuiCol_SliderGrabActive] = ImColor(255, 172, 19);
		style.Colors[ImGuiCol_ScrollbarBg] = ImColor(120, 120, 120);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(255, 172, 19);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrab);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrab);
		style.Colors[ImGuiCol_Header] = ImColor(160, 160, 160);
		style.Colors[ImGuiCol_HeaderHovered] = ImColor(200, 200, 200);
		style.Colors[ImGuiCol_Button] = ImColor(180, 180, 180);
		style.Colors[ImGuiCol_ButtonHovered] = ImColor(200, 200, 200);
		style.Colors[ImGuiCol_ButtonActive] = ImColor(230, 230, 230);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.34f, 0.34f, 0.34f, 1.f);
		style.Colors[ImGuiCol_WindowBg] = ImColor(220, 220, 220, 170);
		style.Colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(1.00f, 1.00f, 1.00f, 0.83f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.40f, 0.40f, 0.80f, 0.20f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.75f, 0.75f, 0.75f, 0.87f);
		style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.40f, 0.40f, 0.55f, 0.80f);
		style.Colors[ImGuiCol_Border] = ImVec4(0.72f, 0.72f, 0.72f, 0.70f);
		style.Colors[ImGuiCol_Text] = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
		style.Colors[ImGuiCol_ChildBg] = ImVec4(0.72f, 0.72f, 0.72f, 0.76f);
		style.Colors[ImGuiCol_PopupBg] = ImVec4(0.76f, 0.76f, 0.76f, 1.00f);
		style.Colors[ImGuiCol_HeaderActive] = ImVec4(0.81f, 0.81f, 0.81f, 1.00f);
		style.Colors[ImGuiCol_Tab] = ImVec4(0.61f, 0.61f, 0.61f, 0.79f);
		style.Colors[ImGuiCol_TabHovered] = ImVec4(0.71f, 0.71f, 0.71f, 0.80f);
		style.Colors[ImGuiCol_TabActive] = ImVec4(0.77f, 0.77f, 0.77f, 0.84f);
		style.Colors[ImGuiCol_TabUnfocused] = ImVec4(0.73f, 0.73f, 0.73f, 0.82f);
		style.Colors[ImGuiCol_TabUnfocusedActive] = ImVec4(0.58f, 0.58f, 0.58f, 0.84f);
		style.Colors[ImGuiCol_WindowBg].w = 0.666f;
	}
	else
	{
		ImGui::StyleColorsClassic();
		style.Colors[ImGuiCol_FrameBg] = ImVec4(0.12f, 0.12f, 0.12f, 0.542f);
		style.Colors[ImGuiCol_FrameBgHovered] = ImColor(20, 20, 20);
		style.Colors[ImGuiCol_FrameBgActive] = ImColor(30, 30, 30);
		style.Colors[ImGuiCol_Separator] = ImVec4(0.031f, 0.031f, 0.031f, 0.279);
		style.Colors[ImGuiCol_CheckMark] = ImColor(255, 172, 19);
		style.Colors[ImGuiCol_SliderGrab] = ImColor(255, 172, 19);
		style.Colors[ImGuiCol_SliderGrabActive] = ImColor(255, 172, 19);
		style.Colors[ImGuiCol_PopupBg] = ImColor(12, 12, 12);
		style.Colors[ImGuiCol_ScrollbarBg] = ImColor(12, 12, 12);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImColor(255, 172, 19);
		style.Colors[ImGuiCol_ScrollbarGrab] = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrab);
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrab);
		style.Colors[ImGuiCol_ScrollbarGrabActive] = ImGui::GetStyleColorVec4(ImGuiCol_ScrollbarGrab);
		style.Colors[ImGuiCol_Border] = ImVec4(0.047f, 0.047f, 0.047f, 0.124f);
		style.Colors[ImGuiCol_ChildBg] = ImColor(16, 16, 16, 127);
		style.Colors[ImGuiCol_Header] = ImColor(16, 16, 16);
		style.Colors[ImGuiCol_HeaderHovered] = ImColor(20, 20, 20);
		style.Colors[ImGuiCol_HeaderActive] = ImColor(30, 30, 30);
		style.Colors[ImGuiCol_Button] = ImColor(8, 8, 8);
		style.Colors[ImGuiCol_ButtonHovered] = ImColor(20, 20, 20);
		style.Colors[ImGuiCol_ButtonActive] = ImColor(30, 30, 30);
		style.Colors[ImGuiCol_Text] = ImColor(207, 207, 207);
		style.Colors[ImGuiCol_TextDisabled] = ImVec4(0.54f, 0.54f, 0.54f, 1.f);
		style.Colors[ImGuiCol_TitleBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.83f);
		style.Colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.0f, 0.0f, 0.0f, 0.20f);
		style.Colors[ImGuiCol_TitleBgActive] = ImVec4(0.f, 0.f, 0.f, 0.87f);
		style.Colors[ImGuiCol_WindowBg] = ImVec4(0.05f, 0.05f, 0.05f, 0.666f);
		style.Colors[ImGuiCol_WindowBg].w = 0.666f;
	}
}
