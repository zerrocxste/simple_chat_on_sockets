namespace Chat
{
	namespace GuiPresents
	{
		namespace Widgets
		{
			void Title(const char* szTitle);
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