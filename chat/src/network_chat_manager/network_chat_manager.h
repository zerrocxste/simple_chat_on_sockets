enum MSG_TYPE
{
	NONE_MSG,
	CHAT_MSG,
	ADMIN_REQUEST,
	ADMIN_STATUS,
	DELETE_MSG
};

class CNetworkChatManager
{
public:
	CNetworkChatManager(bool IsHost, char* szUsername, char* pszIP, int iPort, int iMaxProcessedUsersNumber);
	~CNetworkChatManager();

	bool Initialize();
	void Shutdown();
	void ReceivePacketsRoutine();
	bool SendChatMessage(char* szMessage);
	CChatData* GetChatData();
	size_t GetChatArraySize();
	bool IsNeedExit();
	bool IsHost();
	void AddUser(int m_ID, int m_IP, int m_Port);
	void ResendLastMessagesToClient(int ID, int iNumberToResend);
	bool RequestAdmin(char* szLogin, char* szPassword);
private:
	bool m_bIsInitialized;
	bool m_bIsHost;
	bool m_bIsAdmin;
	bool m_bNeedExit;
	int m_iMaxProcessedUsersNumber;
	int m_iMessageCount;
	CNetwork* m_pNetwork;
	CChatData* m_pChatData;
	char m_szUsername[32];
	std::map<int, chat_user> m_vUsersList;

	int* PacketReadInteger(char* pData, int* pReadCount);
	char* PacketReadString(char* pData, int iStrLength, int* pReadCount);
	char* PacketReadNetString(char* pData, int* pReadCount, int* pStrLength = nullptr);
	MSG_TYPE PacketReadMsgType(char* pData, int* pReadCount);
	bool SendNetMsg(MSG_TYPE MsgType, char* szAuthor, char* szMessage, int iMessageSize, int iMessageID = UNTRACKED_MESSAGE, std::vector<int>* IDList = nullptr);
	bool SendLocalMsg(char* szAuthor, char* szMessage, int iMessageID);
	MSG_TYPE GetMsgType(char* szMsg);
	bool GrantAdmin(char* szLogin, char* szPassword, int iConnectionID);
	bool DeleteChatMessage(std::vector<int>* MsgsList);	
	bool SendStatusAdmin(int ID, bool IsGranted);
};

