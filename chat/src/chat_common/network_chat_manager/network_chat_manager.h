enum MSG_TYPE
{
	MSG_NONE,
	MSG_SEND_CHAT_TO_HOST,
	MSG_SEND_CHAT_TO_CLIENT,
	MSG_ADMIN_REQUEST,
	MSG_ADMIN_STATUS,
	MSG_CONNECTED,
	MSG_CLIENT_CHAT_USER_ID,
	MSG_DELETE,
	MSG_ONLINE_LIST
};

struct chat_user
{
	bool m_bConnected;
	int m_IP;
	int m_iPort;
	bool m_bIsAdmin;
	char m_szUsername[MAX_USERNAME_SIZE];
};

class CNetworkChatManager
{
public:
	CNetworkChatManager(bool IsHost, char* szUsername, char* pszIP, int iPort, int iMaxProcessedUsersNumber);
	~CNetworkChatManager();

	bool Initialize();
	void Shutdown();
	void ReceivePacketsRoutine();
	void ReceiveSendChatToHost(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize);
	void ReceiveSendChatToClient(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize);
	void ReceiveAdminRequest(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize);
	void ReceiveAdminStatus(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize);
	void ReceiveConnected(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize);
	void ReceiveClientChatUserID(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize);
	void ReceiveDelete(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize);
	void ReceiveOnlineList(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize);
	bool SendChatMessage(char* szMessage);
	size_t GetChatArraySize();
	bool IsNeedExit();
	bool IsHost();
	void AddUser(unsigned int ID, int IP, int Port);
	void ResendLastMessagesToClient(unsigned int ID, int iNumberToResend);
	bool DisconnectUser(unsigned int ID);
	bool RequestAdmin(char* szLogin, char* szPassword);
	bool SendConnectedMessage(unsigned int ID = 0);
	bool DeleteChatMessage(std::vector<int>* MsgsList);
	bool IsAdmin();
	unsigned int GetActiveUsers();
	void SendActiveUsersToClients();
	CChatData* GetChatData();
	unsigned int GetClientConnectionID();

	static bool OnConnectionNotification(bool bIsPreConnectionStep, unsigned int iConnectionCount, int iIp, char* szIP, int iPort, NotificationCallbackUserDataPtr pUserData);
	static bool OnDisconnectionNotification(unsigned int iConnectionCount, NotificationCallbackUserDataPtr pUserData);
private:
	bool m_bIsInitialized;
	bool m_bIsHost;
	bool m_bIsAdmin;
	bool m_bNeedExit;
	unsigned int m_iClientConnectionID;
	unsigned int m_iUsersConnectedToHost;
	unsigned int m_iUsersConnectedToHostPrevFrame;
	int m_iMaxProcessedUsersNumber;
	int m_iMessageCount;
	CNetwork* m_pNetwork;
	CChatData* m_pChatData;
	char m_szUsername[32];
	int m_iUsernameLength;
	std::map<unsigned int, chat_user> m_vUsersList;

	char PacketReadChar(char* pData, int* pReadCount);
	int PacketReadInteger(char* pData, int* pReadCount);
	char* PacketReadString(char* pData, int iStrLength, int* pReadCount);
	char* PacketReadNetString(char* pData, int* pReadCount, int* pStrLength = nullptr);
	void PacketWriteChar(char* pData, int* pWriteCount, char cValue);
	void PacketWriteInteger(char* pData, int* pWriteCount, int iValue);
	void PacketWriteString(char* pData, int* pWriteCount, char* szValue, int iValueLength);
	void PacketWriteData(char* pData, int* pWriteCount, char* szValue, int iValueLength);
	void PacketWriteNetString(char* pData, int* pWriteCount, char* szValue, int iValueLength);
	bool IsValidStrMessage(char* szString, int iStringLength);
	bool SendChatMessageToHost(char* szMessage);
	bool SendChatMessageToClient(char* szUsername, char* szMessage, unsigned int iMessageOwnerID, int iMessageID = UNTRACKED_MESSAGE, bool bMessageIsImportant = true, std::vector<unsigned int>* IDList = nullptr);
	bool SendNewChatMessageToClient(char* szUsername, char* szMessage, unsigned int iMessageOwnerID, std::vector<unsigned int>* IDList = nullptr);
	int CalcNetData(int iValueLength);
	int CalcNetString(int iValueLength);
	MSG_TYPE PacketReadMsgType(char* pData, int* pReadCount);
	bool SendNetMsg(MSG_TYPE MsgType, char* szAuthor, int iMessageSize, std::vector<unsigned int>* IDList = nullptr);
	bool SendLocalMsg(char* szAuthor, char* szMessage, int iMessageID);
	MSG_TYPE GetMsgType(char* szMsg);
	const char* GetStrMsgType(MSG_TYPE MsgType);
	bool GrantAdmin(char* szLogin, char* szPassword, unsigned int iConnectionID);
	bool SendStatusAdmin(unsigned int ID, bool IsGranted);
	CNetwork* GetNetwork();
	chat_user* GetUser(unsigned int ID);
};

