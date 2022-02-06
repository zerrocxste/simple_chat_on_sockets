#include "../includes.h"

static APP_MODE AppMode = NOT_INITIALIZED;
static GUI_MODE ChatModePage = SELECT_MODE;
static bool bIsNextHost = false;
static char szUsername[MAX_USERNAME_SIZE] = { NULL };

void Chat::NetworkShutdown()
{
	g_pNetworkChatManager->Shutdown();
}

bool Chat::NetworkHandle()
{
	if (g_pNetworkChatManager->IsNeedExit())
	{
		g_pNetworkChatManager.reset();
		g_pNetworkChatManager = nullptr;
		AppMode = NOT_INITIALIZED;
		printf("[+] %s -> Connection shutdowned\n", __FUNCTION__);
		return false;
	}

	g_pNetworkChatManager->ReceivePacketsRoutine();

	return true;
}

void Chat::StartupNetwork(bool IsHost, char szUsername[32])
{
	ChatModePage = SELECT_MODE;
	AppMode = PROCESS_INITIALIZING;

	g_pNetworkChatManager = std::make_unique<CNetworkChatManager>(IsHost, szUsername, (char*)"127.0.0.1", 80, MAX_PROCESSED_USERS_IN_CHAT);
	memset(szUsername, 0, 32);

	printf("[+] %s -> Start initialize network at: %s\n", __FUNCTION__, IsHost ? "HOST" : "CLIENT");

	if (!g_pNetworkChatManager->Initialize())
	{
		printf("[-] %s -> Failed initialize network!\n", __FUNCTION__);
		g_pNetworkChatManager.release();
		AppMode = FAILED_INITIALIZING;
		return;
	}

	printf("[+] %s -> Network initialized. %s.get(): 0x%p\n", __FUNCTION__, VAR_NAME(g_pNetworkChatManager), g_pNetworkChatManager.get());

	IsHost ? AppMode = HOST : AppMode = CLIENT;
}

void Chat::Run(bool* baBackButton)
{
	if (AppMode == NOT_INITIALIZED) {

		if (!GuiPresents::GuiSelectChatMode(baBackButton))
			return;

		StartupNetwork(bIsNextHost, szUsername);
	}
	else
	{
		if (!NetworkHandle())
			return;

		auto NotInitialized = AppMode == PROCESS_INITIALIZING || AppMode == FAILED_INITIALIZING;

		if (NotInitialized) {
			GuiPresents::GuiAwaitInitialize(baBackButton);
			return;
		}

		GuiPresents::GuiChat(baBackButton);
	}
}

void Chat::GuiPresents::GuiChat(bool* baBackButton)
{
	auto vContentRegionAvail = ImGui::GetContentRegionAvail();

	baBackButton[BACK_BUTTON_ALLOWED] = true;

	if (baBackButton[BACK_BUTTON_PRESSED])
		NetworkShutdown();

	auto szTitle = AppMode == HOST ? "Chat | Host mode" : "Chat";
	Widgets::Title(szTitle);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.f);

	ImGui::BeginChild("##ChatBox", ImVec2(vContentRegionAvail.x, vContentRegionAvail.y - 93.f));
	{
		for (auto it = g_pNetworkChatManager->GetChatData()->Begin(); it != g_pNetworkChatManager->GetChatData()->End(); it++)
		{
			auto message = *it;
			ImGui::Text("%s: %s", message->m_szUsername, message->m_szMessage);
		}
	}
	ImGui::EndChild();

	bool bSendMessage = false;
	static char szBuff[MAX_MESSAGE_TEXT_SIZE] = { NULL };

	const static auto vSendButtonSize = ImVec2(50.f, 25.f);

	ImGui::SetNextItemWidth(vContentRegionAvail.x - vSendButtonSize.x - 5.f);
	if (ImGui::InputText("##Input", szBuff, MAX_MESSAGE_TEXT_SIZE, ImGuiInputTextFlags_::ImGuiInputTextFlags_EnterReturnsTrue))
		bSendMessage = true;

	auto InputItemID = ImGui::GetItemID();

	ImGui::SameLine();

	if (ImGui::Button("Send", vSendButtonSize))
		bSendMessage = true;

	if (bSendMessage
		&& strlen(szBuff))
	{
		g_pNetworkChatManager->SendNewMessage(szBuff);
		memset(szBuff, 0, MAX_MESSAGE_TEXT_SIZE);
		ImGui::ActivateItem(InputItemID);
	}
}

bool Chat::GuiPresents::GuiSelectMode(bool* pbOutIsHost)
{
	auto vContentRegionAvail = ImGui::GetContentRegionMax();

	auto ret = false;

	const ImVec2 vButtonSize = ImVec2(150.f, 90.f);

	ImGui::SetCursorPos(ImVec2(vContentRegionAvail.x * 0.5f - vButtonSize.x, (vContentRegionAvail.y * 0.5f) - (vButtonSize.y * 0.5f)));

	if (ImGui::Button("Host", vButtonSize))
	{
		ret = true;
		*pbOutIsHost = true;
	}

	ImGui::SameLine();

	if (ImGui::Button("Client", vButtonSize))
	{
		ret = true;
		*pbOutIsHost = false;
	}

	return ret;
}

bool Chat::GuiPresents::GuiEnterUsername(char* szOutUsername, bool* baBackButton, GUI_MODE* ChatModePage)
{
	baBackButton[BACK_BUTTON_ALLOWED] = true;

	auto ret = false;

	auto vContentRegionAvail = ImGui::GetContentRegionMax();

	const ImVec2 vButtonSize = ImVec2(80.f, 40.f);

	bool bSendUsername = false;

	const auto flTextInputSize = 250.f;
	ImGui::SetNextItemWidth(flTextInputSize);
	ImGui::SetCursorPos(ImVec2(vContentRegionAvail.x / 2.f - flTextInputSize / 2.f, vContentRegionAvail.y / 2.f - 10.f));

	if (ImGui::InputText("Username", szOutUsername, MAX_USERNAME_SIZE, ImGuiInputTextFlags_::ImGuiInputTextFlags_EnterReturnsTrue))
		bSendUsername = true;

	ImGui::SetCursorPosX(vContentRegionAvail.x / 2.f - vButtonSize.x / 2.f);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30.f);

	if (ImGui::Button("Enter", vButtonSize))
		bSendUsername = true;

	if (bSendUsername && strlen(szOutUsername) > 0)
		ret = true;

	if (baBackButton[BACK_BUTTON_PRESSED])
	{
		memset(szOutUsername, 0, MAX_USERNAME_SIZE);
		*ChatModePage = SELECT_MODE;
	}

	return ret;
}

bool Chat::GuiPresents::GuiSelectChatMode(bool* baBackButton)
{
	auto ret = false;

	Widgets::Title("Start as");

	if (ChatModePage == SELECT_MODE)
	{
		if (GuiSelectMode(&bIsNextHost))
			ChatModePage = ENTER_USERNAME;
	}
	else if (ChatModePage == ENTER_USERNAME) {
		ret = GuiEnterUsername(szUsername, baBackButton, &ChatModePage);
	}

	return ret;
}

void Chat::GuiPresents::GuiAwaitInitialize(bool* baBackButton)
{
	auto vContentRegionAvail = ImGui::GetContentRegionAvail();

	const char* szMessage = AppMode == PROCESS_INITIALIZING ? "Network initializing..." : "Network initialize failed!";

	auto text_size = ImGui::CalcTextSize(szMessage);
	ImGui::SetCursorPos(ImVec2(vContentRegionAvail.x / 2.f - text_size.x / 2.f, vContentRegionAvail.y / 2.f - text_size.y / 2.f));

	ImGui::Text(szMessage);

	if (AppMode == FAILED_INITIALIZING)
	{
		ImVec2 vButtonSize = ImVec2(70.f, 40.f);
		ImGui::ItemSize(ImVec2(0.f, 30.f));
		ImGui::SetCursorPosX(vContentRegionAvail.x / 2.f - vButtonSize.x / 2.f);
		if (ImGui::Button("Back", vButtonSize))
			AppMode = NOT_INITIALIZED;
	}
}

void Chat::GuiPresents::Widgets::Title(const char* szTitle)
{
	auto vCursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPos(ImVec2(vCursorPos.x + 5.f, vCursorPos.y + 20.f));
	ImGui::PushFont(Gui::fTitle);
	ImGui::Text(szTitle);
	ImGui::PopFont();
}