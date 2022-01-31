class CNetworkChatManager
{
public:
	CNetworkChatManager(bool IsHost, char* szUsername, char* pszIP, int iPort);
	~CNetworkChatManager();

	bool Initialize();
	void Shutdown();
	void ReceivePacketsRoutine();
	bool SendNewMessage(char* szauthor, char* szmessage);
	bool SendNewMessage(char* szmessage);
	CChatData* GetChatData();
	size_t GetChatArraySize();
	bool IsNeedExit();
	bool IsHost();
private:
	bool m_bIsInitialized;
	bool m_bIsHost;
	bool m_bNeedExit;
	CNetwork* m_pNetwork;
	CChatData* m_pChatData;
	char m_szUsername[32];
};

