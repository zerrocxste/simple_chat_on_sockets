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
	chat_packet_data_t(int iMessageLength);
	chat_packet_data_t(char* pPacket, int iMessageLength);
	~chat_packet_data_t();
	operator bool() const;
	bool operator!() const;
	void operator=(const chat_packet_data_t& PacketData);
private:
	mutable char* m_pPacket;
	int m_iMessageLength;
};

class CNetworkChatManager
{
public:
	CNetworkChatManager(bool IsHost, char* szUsername, char* pszIP, int iPort, netconnectcount iMaxProcessedUsersNumber);
	~CNetworkChatManager();

	bool Initialize();
	void Shutdown();
	void ReceivePacketsRoutine();
	void ReceiveSendChatToHost(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData);
	void ReceiveSendChatToClient(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData);
	void ReceiveAdminRequest(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData);
	void ReceiveAdminStatus(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData);
	void ReceiveConnected(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData);
	void ReceiveClientChatUserID(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData);
	void ReceiveDelete(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData, int iDataSize);
	void ReceiveOnlineList(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData);
	void SendChatMessage(char* szMessage);
	size_t GetChatArraySize();
	bool IsNeedExit();
	bool IsHost();
	void AddUser(netconnectcount iConnectionID, int IP, int Port);
	void ResendLastMessagesToClient(netconnectcount iConnectionID, int iNumberToResend);
	bool DisconnectUser(netconnectcount iConnectionID);
	bool RequestAdmin(char* szLogin, char* szPassword);
	void SendConnectedMessage(netconnectcount iConnectionID = 0);
	bool DeleteChatMessage(std::vector<int>* MsgsList);
	bool IsAdmin();
	netconnectcount GetActiveUsers();
	void SendActiveUsersToClients();
	CChatData* GetChatData();
	netconnectcount GetClientConnectionID();

	static bool OnConnectionNotification(bool bIsPreConnectionStep, netconnectcount iConnectionID, int iIp, char* szIP, int iPort, NotificationCallbackUserDataPtr pUserData);
	static bool OnDisconnectionNotification(netconnectcount iConnectionID, NotificationCallbackUserDataPtr pUserData);
private:
	bool m_bIsInitialized;
	bool m_bIsHost;
	bool m_bIsAdmin;
	bool m_bNeedExit;
	netconnectcount m_iClientConnectionID;
	netconnectcount m_iUsersConnectedToHost;
	netconnectcount m_iUsersConnectedToHostPrevFrame;
	netconnectcount m_iMaxProcessedUsersNumber;
	int m_iMessageCount;
	CNetworkTCP* m_pNetwork;
	CChatData* m_pChatData;
	char m_szUsername[32];
	int m_iUsernameLength;
	std::map<netconnectcount, chat_user> m_vUsersList;
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

	chat_packet_data_t CreateClientChatMessage(char* szUsername, char* szMessage, netconnectcount iMessageOwnerID, int iMessageID, bool bMessageIsImportant = true);
	bool SendHostChatMessage(char* szMessage);

	int CalcNetData(int iValueLength);
	int CalcNetString(int iValueLength);

	MSG_TYPE GetMsgType(char* szMsg);
	const char* GetStrMsgType(MSG_TYPE MsgType);
	MSG_TYPE PacketReadMsgType(char* pData, int* pReadCount);

	void SendNetMsg(chat_packet_data_t& chat_packet_data);
	void SendNetMsgToID(chat_packet_data_t& chat_packet_data, netconnectcount iConnectionID);
	void SendNetMsgToSocket(chat_packet_data_t& chat_packet_data, CNetworkTCP::NETSOCK Socket);

	bool GrantAdmin(char* szLogin, char* szPassword, netconnectcount iConnectionID);
	void SendStatusAdmin(netconnectcount iConnectionID, bool IsGranted);

	chat_user* GetUser(netconnectcount iConnectionID);

	CNetworkTCP* GetNetwork();
};

