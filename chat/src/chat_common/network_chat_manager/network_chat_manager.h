class CNetworkChatManager;

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

struct chat_packet_data_t
{
	friend CNetworkChatManager;
public:
	chat_packet_data_t();
	chat_packet_data_t(const chat_packet_data_t& PacketData);
	chat_packet_data_t(short iMessageLength);
	chat_packet_data_t(char* pPacket, short iMessageLength);
	~chat_packet_data_t();
	operator bool() const;
	bool operator!() const;
	void operator=(const chat_packet_data_t& PacketData);
private:
	mutable char* m_pPacket;
	short m_iMessageLength;
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
	void SendChatMessage(char* szMessage);
	size_t GetChatArraySize();
	bool IsNeedExit();
	bool IsHost();
	void AddUser(unsigned int ID, int IP, int Port);
	void ResendLastMessagesToClient(unsigned int iConnectionID, int iNumberToResend);
	bool DisconnectUser(unsigned int iConnectionID);
	bool RequestAdmin(char* szLogin, char* szPassword);
	void SendConnectedMessage(unsigned int iConnectionID = 0);
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
	CNetworkTCP* m_pNetwork;
	CChatData* m_pChatData;
	char m_szUsername[32];
	int m_iUsernameLength;
	std::map<unsigned int, chat_user> m_vUsersList;
	std::mutex m_mtxChatData;

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

	void IncreaseMessagesCounter();

	chat_packet_data_t CreateNetMsg(MSG_TYPE MsgType, char* szMessage, int iMessageSize);

	chat_packet_data_t CreateClientChatMessage(char* szUsername, char* szMessage, unsigned int iMessageOwnerID, int iMessageID, bool bMessageIsImportant = true);
	bool SendHostChatMessage(char* szMessage);
	
	int CalcNetData(int iValueLength);
	int CalcNetString(int iValueLength);

	MSG_TYPE GetMsgType(char* szMsg);
	const char* GetStrMsgType(MSG_TYPE MsgType);
	MSG_TYPE PacketReadMsgType(char* pData, int* pReadCount);
	
	void SendNetMsg(chat_packet_data_t& chat_packet_data);
	void SendNetMsg(chat_packet_data_t& chat_packet_data, unsigned int iConnectionID);
	void SendNetMsg(chat_packet_data_t& chat_packet_data, CNetworkTCP::NETSOCK Socket);
	bool SendLocalMsg(char* szAuthor, char* szMessage, int iMessageID);

	bool GrantAdmin(char* szLogin, char* szPassword, unsigned int iConnectionID);
	void SendStatusAdmin(unsigned int iConnectionID, bool IsGranted);
	
	chat_user* GetUser(unsigned int ID);

	CNetworkTCP* GetNetwork();
};

