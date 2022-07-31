enum APP_MODE
{
	NOT_INITIALIZED,
	PROCESS_INITIALIZING,
	FAILED_INITIALIZING,
	HOST,
	CLIENT
};

enum GUI_MODE
{
	SELECT_MODE,
	ENTER_USERNAME
};

namespace Chat
{
	namespace GuiPresents
	{
		namespace Widgets
		{
			ImVec2 Title(const char* szTitle);
		}
		void GuiChat(bool* baBackButton);
		bool GuiSelectMode(bool* pbOutIsHost);
		bool GuiEnterUsername(char* szOutUsername, bool* baBackButton, GUI_MODE* ChatModePage);
		bool GuiSelectChatMode(bool* baBackButton);
		void GuiAwaitInitialize(bool* baBackButton);
	}
	void Run(bool* baBackButton);
	void NetworkShutdown();
	bool NetworkHandle();
	void StartupNetwork(bool IsHost, char szUsername[32]);
}