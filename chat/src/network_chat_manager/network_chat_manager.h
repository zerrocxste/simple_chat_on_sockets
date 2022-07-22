enum MSG_TYPE
{
	MSG_NONE,
	MSG_SEND_CHAT_TO_HOST,
	MSG_SEND_CHAT_TO_CLIENT,
	MSG_ADMIN_REQUEST,
	MSG_ADMIN_STATUS,
	MSG_CONNECTED,
	MSG_DELETE,
	MSG_ONLINE_LIST
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
	size_t GetChatArraySize();
	bool IsNeedExit();
	bool IsHost();
	void AddUser(int m_ID, int m_IP, int m_Port);
	void ResendLastMessagesToClient(int ID, int iNumberToResend);
	bool DisconnectUser(int ID);
	bool RequestAdmin(char* szLogin, char* szPassword);
	bool SendConnectedMessage(int ID = 0);
	bool DeleteChatMessage(std::vector<int>* MsgsList);
	bool IsAdmin();
	int GetActiveUsers();
	void SendActiveUsersToClients();
	CChatData* GetChatData();
private:
	bool m_bIsInitialized;
	bool m_bIsHost;
	bool m_bIsAdmin;
	bool m_bNeedExit;
	int m_iUsersConnectedToHost;
	int m_iMaxProcessedUsersNumber;
	int m_iMessageCount;
	CNetwork* m_pNetwork;
	CChatData* m_pChatData;
	char m_szUsername[32];
	int m_iUsernameLength;
	std::map<int, chat_user> m_vUsersList;

	int PacketReadInteger(char* pData, int* pReadCount);
	char* PacketReadString(char* pData, int iStrLength, int* pReadCount);
	char* PacketReadNetString(char* pData, int* pReadCount, int* pStrLength = nullptr);
	void PacketWriteInteger(char* pData, int* pWriteCount, int iValue);
	void PacketWriteString(char* pData, int* pWriteCount, char* szValue, int iValueLength);
	void PacketWriteData(char* pData, int* pWriteCount, char* szValue, int iValueLength);
	void PacketWriteNetString(char* pData, int* pWriteCount, char* szValue, int iValueLength);
	bool IsValidStrMessage(char* szString, int iStringLength);
	bool SendChatMessageToHost(char* szMessage);
	bool SendChatMessageToClient(char* szUsername, char* szMessage, int iMessageID = UNTRACKED_MESSAGE, std::vector<int>* IDList = nullptr);
	bool SendNewChatMessageToClient(char* szUsername, char* szMessage, std::vector<int>* IDList = nullptr);
	int CalcNetData(int iValueLength);
	int CalcNetString(int iValueLength);
	MSG_TYPE PacketReadMsgType(char* pData, int* pReadCount);
	bool SendNetMsg(MSG_TYPE MsgType, char* szAuthor, int iMessageSize, std::vector<int>* IDList = nullptr);
	bool SendLocalMsg(char* szAuthor, char* szMessage, int iMessageID);
	MSG_TYPE GetMsgType(char* szMsg);
	const char* GetStrMsgType(MSG_TYPE MsgType);
	bool GrantAdmin(char* szLogin, char* szPassword, int iConnectionID);
	bool SendStatusAdmin(int ID, bool IsGranted);
	CNetwork* GetNetwork();
	chat_user* GetUser(int ID);
};

