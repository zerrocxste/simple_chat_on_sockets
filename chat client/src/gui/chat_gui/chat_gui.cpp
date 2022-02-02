#include "../../includes.h"

APP_MODE AppMode = NOT_INITIALIZED;

void NetworkHandleThread(void* arg)
{
	auto NetworkThreadArg = (network_thread_arg*)arg;

	auto IsHost = NetworkThreadArg->bIsHost;
	auto szUsername = NetworkThreadArg->szUsername;

	g_pNetworkChatManager = std::make_unique<CNetworkChatManager>(IsHost, szUsername, (char*)"127.0.0.1", 80, MAX_PROCESSED_USERS_IN_CHAT);

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

	delete NetworkThreadArg;

	while (!g_pNetworkChatManager->IsNeedExit())
	{
		g_pNetworkChatManager->ReceivePacketsRoutine();
		std::this_thread::sleep_for(std::chrono::microseconds(1));
	}

	g_pNetworkChatManager.reset();
	g_pNetworkChatManager = nullptr;

	AppMode = NOT_INITIALIZED;

	printf("[+] %s -> Connection shutdowned\n", __FUNCTION__);
}

void ChatGui::StartupNetwork(bool IsHost, char szUsername[32])
{
	network_thread_arg* pNetWorkThreadArg = new network_thread_arg;

	pNetWorkThreadArg->bIsHost = IsHost;
	memcpy(pNetWorkThreadArg->szUsername, szUsername, MAX_USERNAME_SIZE);

	auto th = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)NetworkHandleThread, pNetWorkThreadArg, 0, nullptr);

	if (th)
		CloseHandle(th);
}

void ChatGui::Draw(bool* baBackButton)
{
	if (AppMode == NOT_INITIALIZED)
		ChatGui::SelectChatMode();
	else
		ChatGui::DrawChat(baBackButton);
}

bool ChatGui::GuiSelectMode(bool* pbOutIsHost)
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

bool ChatGui::GuiEnterUsername(char* szOutUsername)
{
	auto ret = false;

	auto vContentRegionAvail = ImGui::GetContentRegionMax();

	const ImVec2 vButtonSize = ImVec2(80.f, 40.f);

	bool bSendUsername = false;

	const auto flTextInputSize = 250.f;
	ImGui::SetNextItemWidth(flTextInputSize);
	ImGui::SetCursorPos(ImVec2(vContentRegionAvail.x / 2.f - flTextInputSize / 2.f, vContentRegionAvail.y / 2.f - 10.f));
	if (ImGui::InputText("Username", szOutUsername, 32, ImGuiInputTextFlags_::ImGuiInputTextFlags_EnterReturnsTrue))
		bSendUsername = true;

	ImGui::SetCursorPosX(vContentRegionAvail.x / 2.f - vButtonSize.x / 2.f);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 30.f);
	if (ImGui::Button("Enter", vButtonSize))
		bSendUsername = true;

	if (bSendUsername && strlen(szOutUsername) > 0)
		ret = true;

	return ret;
}

void ChatGui::SelectChatMode()
{
	Widgets::Title("Start as");

	enum
	{
		SELECT_MODE,
		ENTER_USERNAME
	};

	static int ChatModePage = SELECT_MODE;
	static bool bIsNextHost = false;
	static char szUsername[MAX_USERNAME_SIZE] = { NULL };

	if (ChatModePage == SELECT_MODE)
	{
		if (!GuiSelectMode(&bIsNextHost))
			return;

		ChatModePage = ENTER_USERNAME;
		return;
	}
	else if (ChatModePage == ENTER_USERNAME)
	{
		if (!GuiEnterUsername(szUsername))
			return;
	}

	ChatModePage = SELECT_MODE;
	AppMode = PROCESS_INITIALIZING;
	StartupNetwork(bIsNextHost, szUsername);
	memset(szUsername, 0, 32);
}

void ChatGui::DrawChat(bool* baBackButton)
{
	auto vContentRegionAvail = ImGui::GetContentRegionAvail();

	if (AppMode == PROCESS_INITIALIZING || AppMode == FAILED_INITIALIZING)
	{
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
		return;
	}

	baBackButton[BACK_BUTTON_ALLOWED] = true;
	
	if (baBackButton[BACK_BUTTON_PRESSED])
	{
		g_pNetworkChatManager->Shutdown();
		AppMode = NOT_INITIALIZED;
	}

	Widgets::Title("Chat");
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.f);

	ImGui::BeginChild("##ChatBox", ImVec2(vContentRegionAvail.x, vContentRegionAvail.y - 93.f));
	{
		for (auto it = g_pNetworkChatManager->GetChatData()->Begin(); it != g_pNetworkChatManager->GetChatData()->End(); it++)
		{
			auto message = **it;
			ImGui::Text("[%s] %s", message->m_szUsername, message->m_szMessage);
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

void ChatGui::Widgets::Title(const char* szTitle)
{
	auto vCursorPos = ImGui::GetCursorPos();
	ImGui::SetCursorPos(ImVec2(vCursorPos.x + 5.f, vCursorPos.y + 20.f));
	ImGui::PushFont(Gui::fTitle);
	ImGui::Text(szTitle);
	ImGui::PopFont();	
}