class CNetworkChatManager
{
public:
	CNetworkChatManager(bool IsHost, char* szUsername, char* pszIP, int iPort, int iMaxProcessedUsersNumber);
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
	int m_iMaxProcessedUsersNumber;
	CNetwork* m_pNetwork;
	CChatData* m_pChatData;
	char m_szUsername[32];
};

