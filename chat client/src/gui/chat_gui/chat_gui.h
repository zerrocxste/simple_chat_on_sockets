namespace ChatGui
{
	void Draw(bool* baBackButton);
	bool GuiSelectMode(bool* pbOutIsHost);
	bool GuiEnterUsername(char* szOutUsername);
	void SelectChatMode();
	void StartupNetwork(bool IsHost, char szUsername[32]);
	void DrawChat(bool* baBackButton);
	namespace Widgets
	{
		void Title(const char* szTitle);
	}
}