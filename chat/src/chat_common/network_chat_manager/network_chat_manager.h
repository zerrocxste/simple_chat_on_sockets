class CNetworkChatManager;
class ICustomChatHandler;

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
	void reset_process_step();
	void release();
private:
	mutable char* m_pPacket;
	int m_iProcessStep;
	int m_iMessageLength;
};

template <class T> class CSafeObject
{
private:
	std::recursive_mutex* m;
	T* o;
public:
	CSafeObject(T* obj, std::recursive_mutex* mtx) : o(obj), m(mtx)
	{
		m->lock();
	}

	~CSafeObject()
	{
		m->unlock();
	}

	T* Get()
	{
		return this->o;
	}

	T* operator->()
	{
		return this->o;
	}
};

class CNetworkChatManager : public IError
{
	friend ICustomChatHandler;
public:
	CNetworkChatManager(bool IsHost, char* szUsername, char* pszIP, int iPort, netconnectcount iMaxProcessedUsersNumber);
	~CNetworkChatManager();

	bool Initialize();
	void Shutdown();
	void ForceExit();
	void ReceivePacketsRoutine();
	void ReceiveSendChatToHost(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData);
	void ReceiveSendChatToHost(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID);
	void ReceiveSendChatToClient(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData);
	void ReceiveSendChatToClient(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID);
	void ReceiveAdminRequest(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID);
	void ReceiveAdminRequest(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData);
	void ReceiveAdminStatus(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData);
	void ReceiveAdminStatus(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID);
	void ReceiveClientChatUserID(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID);
	void ReceiveClientChatUserID(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData);
	void ReceiveConnected(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData);
	void ReceiveConnected(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID);
	void ReceiveDelete(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData, int iDataSize);
	void ReceiveDelete(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID);
	void ReceiveOnlineList(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData);
	void ReceiveOnlineList(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID);
	void SendChatMessage(char* szMessage);
	void AddCustomChatHandler(ICustomChatHandler* pCustomChatHandler);
	size_t GetChatArraySize();
	bool IsNeedExit();
	bool IsHost();
	void AddUser(netconnectcount iConnectionID, int IP, int Port);
	void ResendLastMessagesToClient(netconnectcount iConnectionID, int iNumberToResend);
	bool DisconnectUser(netconnectcount iConnectionID);
	bool KickUser(netconnectcount iConnectionID);
	bool RequestAdmin(char* szLogin, char* szPassword);
	void SendConnectedMessage(netconnectcount iConnectionID = 0);
	bool DeleteChatMessage(std::vector<int>* MsgsList);
	bool IsAdmin();
	netconnectcount GetActiveUsers();
	void SendActiveUsersToClients();
	CSafeObject<CChatData> GetChatData();
	netconnectcount GetClientConnectionID();
	bool AddAdminAccessLoginPassword(const char szLogin[128], const char szPassword[128]);

	static char PacketReadChar(char* pData, int* pReadCount);
	static int PacketReadInteger(char* pData, int* pReadCount);
	static char* PacketReadData(char* pData, int iReadingSize, int* pReadCount);
	static char* PacketReadString(char* pData, int iStrLength, int* pReadCount);
	static char* PacketReadNetString(char* pData, int* pReadCount, int* pStrLength = nullptr);
	static char PacketReadChar(chat_packet_data_t& packet_data);
	static int PacketReadInteger(chat_packet_data_t& packet_data);
	static char* PacketReadData(chat_packet_data_t& packet_data, int iReadingSize);
	static char* PacketReadString(chat_packet_data_t& packet_data, int iStrLength);
	static char* PacketReadNetString(chat_packet_data_t& packet_data, int* pStrLength = nullptr);

	static void PacketWriteChar(char* pData, int* pWriteCount, char cValue);
	static void PacketWriteInteger(char* pData, int* pWriteCount, int iValue);
	static void PacketWriteString(char* pData, int* pWriteCount, char* szValue, int iValueLength);
	static void PacketWriteData(char* pData, int* pWriteCount, char* szValue, int iValueLength);
	static void PacketWriteNetString(char* pData, int* pWriteCount, char* szValue, int iValueLength);
	static void PacketWriteChar(chat_packet_data_t& packet_data, char cValue);
	static void PacketWriteInteger(chat_packet_data_t& packet_data, int iValue);
	static void PacketWriteString(chat_packet_data_t& packet_data, char* szValue, int iValueLength);
	static void PacketWriteData(chat_packet_data_t& packet_data, char* szValue, int iValueLength);
	static void PacketWriteNetString(chat_packet_data_t& packet_data, char* szValue, int iValueLength);

	static int CalcNetData(int iValueLength);
	static int CalcNetString(int iValueLength);
	static int CalcStrLen(const char* pszStr);

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
	std::recursive_mutex m_mtxChatData;
	ICustomChatHandler* m_pCustomChatMangerHandler;
	struct admin_access_login_password_s
	{
		char m_szLogin[128];
		char m_szPassword[128];
	};
	std::vector<admin_access_login_password_s> m_vLoginPassData;
	bool IsValidStrMessage(char* szString, int iStringLength);
	void IncreaseMessagesCounter();
	chat_packet_data_t CreateNetMsg(const char* szMsgType, int iMessageSize = 0);
	chat_packet_data_t CreateClientChatMessage(char* szUsername, char* szMessage, netconnectcount iMessageOwnerID, int iMessageID, bool bMessageIsImportant = true);
	bool SendHostChatMessage(char* szMessage);
	MSG_TYPE GetMsgType(char* szMsg);
	void CheckPacketValid(chat_packet_data_t& chat_packet_data);
	void SendNetMsg(chat_packet_data_t& chat_packet_data);
	void SendNetMsgIncludeID(chat_packet_data_t& chat_packet_data, netconnectcount iConnectionID);
	void SendNetMsgExcludeID(chat_packet_data_t& chat_packet_data, netconnectcount iConnectionID);
	using fSendConditionSort = bool(*)(netconnectcount, chat_user*);
	void SendNetMsgWithCondition(chat_packet_data_t& chat_packet_data, fSendConditionSort f);
	bool GrantAdmin(char* szLogin, char* szPassword, netconnectcount iConnectionID);
	void SendStatusAdmin(netconnectcount iConnectionID, bool IsGranted);
	chat_user* GetUser(netconnectcount iConnectionID);
	CNetworkTCP* GetNetwork();
};

class ICustomChatHandler
{
	friend CNetworkChatManager;
public:
	~ICustomChatHandler() = default;
protected:
	CNetworkChatManager* m_pNetworkChatHandler;

	chat_packet_data_t CreateNetMsg(const char* szMsgType, int iMessageSize = 0)
	{
		return this->m_pNetworkChatHandler->CreateNetMsg(szMsgType, iMessageSize);
	}

	void SendNetMsg(chat_packet_data_t& chat_packet_data)
	{
		this->m_pNetworkChatHandler->SendNetMsg(chat_packet_data);
	}

	void SendNetMsgInclude(chat_packet_data_t& chat_packet_data, netconnectcount iConnectionID)
	{
		this->m_pNetworkChatHandler->SendNetMsgIncludeID(chat_packet_data, iConnectionID);
	}

	void SendNetMsgExclude(chat_packet_data_t& chat_packet_data, netconnectcount iConnectionID)
	{
		this->m_pNetworkChatHandler->SendNetMsgExcludeID(chat_packet_data, iConnectionID);
	}

	void SendNetMsgWithCondition(chat_packet_data_t& chat_packet_data, CNetworkChatManager::fSendConditionSort f)
	{
		this->m_pNetworkChatHandler->SendNetMsgWithCondition(chat_packet_data, f);
	}
private:
	void OnInit(CNetworkChatManager* pNetworkChatManager)
	{
		this->m_pNetworkChatHandler = pNetworkChatManager;
	}
public:
	virtual void OnRelease() = 0;
	virtual bool OnPreConnectionStep(netconnectcount iConnectionID, int iIp, char* szIP, int iPort) = 0;
	virtual void OnConnectUser(netconnectcount iConnectionID, chat_user* User) = 0;
	virtual void OnDisconnectionUser(netconnectcount iConnectionID, chat_user* chat_user_data) = 0;
	virtual void ReceivePacketRoutine(chat_packet_data_t& packet_data, netconnectcount iConnectionID, chat_user* User) = 0;
	virtual void OnAdminAllowed(netconnectcount iConnectionID, chat_user* User) = 0;
};