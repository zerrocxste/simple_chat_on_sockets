namespace ChatGui
{
	enum
	{
		SELECT_MODE,
		ENTER_USERNAME
	};
	void Draw(bool* baBackButton);
	bool GuiSelectMode(bool* pbOutIsHost);
	bool GuiEnterUsername(char* szOutUsername, bool* baBackButton, int* ChatModePage);
	void SelectChatMode(bool* baBackButton);
	void StartupNetwork(bool IsHost, char szUsername[32]);
	void DrawChat(bool* baBackButton);
	namespace Widgets
	{
		void Title(const char* szTitle);
	}
}