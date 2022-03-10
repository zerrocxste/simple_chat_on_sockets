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

		if (NotInitialized) 
		{
			GuiPresents::GuiAwaitInitialize(baBackButton);
			return;
		}

		GuiPresents::GuiChat(baBackButton);
	}
}

static bool AdminLogin = false;

void Chat::GuiPresents::GuiChat(bool* baBackButton)
{
	auto vContentRegionAvail = ImGui::GetContentRegionAvail();

	baBackButton[BACK_BUTTON_ALLOWED] = true;

	if (baBackButton[BACK_BUTTON_PRESSED])
		NetworkShutdown();

	auto szTitle = AppMode == HOST ? "Chat | Host mode" : "Chat";
	Widgets::Title(szTitle);
	ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 10.f);

	if (AppMode == CLIENT)
	{
		if (!AdminLogin)
		{
			if (GetForegroundWindow() == DXWFGetHWND() && GetAsyncKeyState(VK_MENU) && GetAsyncKeyState('A'))
				AdminLogin = true;
		}

		if (AdminLogin)
		{
			static bool Send = false;

			static char szLogin[32] = { 0 };
			static char szPassword[32] = { 0 };

			ImGui::Text("Sing in admin");

			ImGui::Text("Login:");
			ImGui::InputText("##Login", szLogin, 32);

			ImGui::Text("Password:");
			ImGui::InputText("##Password", szPassword, 32);

			static auto vButtonSize = ImVec2(100.f, 30.f);

			if (ImGui::Button("Login", vButtonSize))
				Send = true;

			ImGui::SameLine();

			if (ImGui::Button("Back", vButtonSize))
				AdminLogin = false;

			if (Send && strlen(szLogin) > 1 && strlen(szPassword) > 1)
			{
				Send = false;
				g_pNetworkChatManager->RequestAdmin(szLogin, szPassword);
				memset(szLogin, 0, 32);
				memset(szPassword, 0, 32);
				AdminLogin = false;
			}
			return;
		}
	}

	static std::vector<int> vSelectedMessages;

	auto IsDeleteMode = !vSelectedMessages.empty();

	//ImGui::PushStyleVar(ImGuiStyleVar_::ImGuiStyleVar_WindowPadding, ImVec2(12.f, 12.f));
	ImGui::BeginChild("##ChatBox", ImVec2(vContentRegionAvail.x, vContentRegionAvail.y - 93.f));
	{
		for (auto itMessage = g_pNetworkChatManager->GetChatData()->Begin(); itMessage != g_pNetworkChatManager->GetChatData()->End(); itMessage++)
		{
			auto message = *itMessage;

			if (!message)
				continue;

			auto ID = message->m_iMessageID;
			auto IDStr = std::string("##") + std::to_string(ID);

			auto fFindInVec = [](std::vector<int>* vSearchStr, int iSearchElement, std::vector<int>::iterator* it) -> bool {

				auto itSearch = std::find(vSearchStr->begin(), vSearchStr->end(), iSearchElement);

				if (itSearch != vSearchStr->end())
				{
					*it = itSearch;
					return true;
				}

				return false;
			};

			std::vector<int>::iterator itSearch;

			auto IsFounded = fFindInVec(&vSelectedMessages, ID, &itSearch);

			if (ImGui::Selectable(IDStr.c_str(), IsFounded))
			{
				if (g_pNetworkChatManager->IsAdmin())
				{
					if (ID != UNTRACKED_MESSAGE)
					{
						if (IsFounded)
						{
							LOGGER("Unselected %d message\n", ID);
							vSelectedMessages.erase(itSearch);
						}
						else
						{
							LOGGER("Selected %d message\n", ID);
							vSelectedMessages.push_back(ID);
						}
					}
				}
			}

			ImGui::SameLine();

			//auto CursorPos = ImGui::GetCursorPos();
			//ImGui::SetCursorPos(ImVec2(CursorPos.x + 5.f, CursorPos.y + 5.f));

			ImGui::Text("[%d] %s: %s", message->m_iMessageID, message->m_szUsername, message->m_szMessage);
		}
	}
	ImGui::EndChild();
	//ImGui::PopStyleVar();

	if (!IsDeleteMode)
	{
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
			g_pNetworkChatManager->SendChatMessage(szBuff);
			memset(szBuff, 0, MAX_MESSAGE_TEXT_SIZE);
			ImGui::ActivateItem(InputItemID);
		}
	}
	else
	{
		bool bCancelDeleteMessages = false;
		bool bDeleteMessages = false;

		auto vSendButtonSize = ImVec2(ImGui::GetContentRegionAvailWidth() * 0.5f - ImGui::GetStyle().ItemInnerSpacing.x, 25.f);

		if (ImGui::Button("Cancel", vSendButtonSize))
			bCancelDeleteMessages = true;

		ImGui::SameLine();

		if (ImGui::Button("Delete", vSendButtonSize))
			bDeleteMessages = true;

		if (bCancelDeleteMessages)
		{
			vSelectedMessages.clear();
		}

		if (bDeleteMessages)
		{
			g_pNetworkChatManager->DeleteChatMessage(&vSelectedMessages);
			vSelectedMessages.clear();
		}
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