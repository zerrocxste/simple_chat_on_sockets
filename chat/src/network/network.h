constexpr auto CLIENT_SOCKET = 0;

struct net_packet_t
{
	net_packet_t() :
		m_pPacket(nullptr),
		m_iSize(0),
		m_iConnectionID(0)
	{

	};

	net_packet_t(void* pPacket, int iSize, int iConnectionID) :
		m_pPacket(pPacket),
		m_iSize(iSize),
		m_iConnectionID(iConnectionID)
	{

	}

	void* m_pPacket;
	int m_iSize;
	int m_iConnectionID;
};

class CNetworkTCP;

using NotificationCallbackUserDataPtr = void*;

using f_ClientConnectionNotification = bool(*)(bool bIsPreConnectionStep, unsigned int iConnectionCount, int iIp, char* szIP, int iPort, NotificationCallbackUserDataPtr pUserData);
using f_ClientDisconnectionNotification = bool(*)(unsigned int iConnectionCount, NotificationCallbackUserDataPtr pUserData);

struct client_receive_data_thread_t
{
	SOCKET m_ConnectionSocket;
	unsigned int m_iThreadId;
	SOCKADDR_IN m_SockAddrIn;
};

struct host_receive_thread_arg_t
{
	CNetworkTCP* m_Network;
	client_receive_data_thread_t* m_CurrentClient;
};

struct delta_packet_t
{
	short magic;
	unsigned short m_iPacketSize;
};

class CNetworkTCP : public IError
{
private:
	static const int iDeltaPacketLength = sizeof(delta_packet_t);
	int m_iSockAddrInLength = sizeof(SOCKADDR_IN);

#ifdef _WIN32
	static int g_iCreatedLinkCount;
	static WSADATA g_WSAdata;
#endif // _WIN32

	template <class T> __forceinline static bool ThreadCreate(T* pfunc, void* arg) noexcept
	{
		std::thread(pfunc, arg).detach();
		return true;
	}

	static void thHostClientReceive(void* arg);
	static void thHostClientsHandling(void* arg);

	static void thClientHostReceive(void* arg);

	bool m_bIsInitialized;
	bool m_bIsHost;
	int m_iMaxProcessedUsersNumber;
	char* m_pszIP;
	int m_IPort;
	SOCKADDR_IN m_SockAddrIn;
	SOCKET m_Socket;
	std::map<unsigned int, client_receive_data_thread_t> m_ClientsList;
	unsigned int m_iConnectionCount;
	std::vector<net_packet_t> m_PacketsList;
	bool m_bServerWasDowned;
	f_ClientConnectionNotification m_pf_ClientConnectionNotificationCallback;
	NotificationCallbackUserDataPtr m_pOnClientConnectionUserData;
	f_ClientDisconnectionNotification m_pf_ClientDisconnectionNotificationCallback;
	NotificationCallbackUserDataPtr m_pOnClientDisconnectionUserData;
	std::mutex m_mtxSendData;
	std::mutex m_mtxExchangePacketsData;

	bool InitializeNetwork();
	bool InitializeAsHost();
	bool InitializeAsClient();
	void AddToPacketList(net_packet_t NetPacket);
	bool InvokeClientConnectionNotification(bool bIsPreConnectionStep, int iConnectionCount, int iIP, char* szIP, int iPort);
	bool InvokeClientDisconnectionNotification(int iConnectionCount);
	bool SendToSocket(SOCKET Socket, void* pPacket, int iSize);
	bool ReceivePacket(net_packet_t* pPacket);
	void ShutdownNetwork();
	void DropConnections();
	void DisconnectSocket(SOCKET Socket);
	void DisconnectClient(client_receive_data_thread_t* Client);
public:
	CNetworkTCP(bool IsHost, char* pszIP, int iPort, int iMaxProcessedUsersNumber);
	~CNetworkTCP();

	bool Startup();
	bool SendPacketAll(void* pPacket, int iSize);
	bool SendPacketExcludeID(void* pPacket, int iSize, std::vector<unsigned int>* vIDList);
	bool SendPacketIncludeID(void* pPacket, int iSize, std::vector<unsigned int>* vIDList);
	bool GetReceivedData(net_packet_t* pPacket);
	bool ServerWasDowned();
	bool IsHost();
	unsigned int GetConnectedUsersCount();
	void DisconnectUser(int IdCount);
	bool GetIpByClientId(int IdCount, int* pIP);
	char* GetStrIpFromSockAddrIn(PSOCKADDR_IN pSockAddrIn);
	int GetIntegerIpFromSockAddrIn(PSOCKADDR_IN pSockAddrIn);
	int GetPortFromSockAddrIn(PSOCKADDR_IN pSockAddrIn);
	bool AddClientsConnectionNotificationCallback(f_ClientConnectionNotification pf_NewClientsNotificationCallback, NotificationCallbackUserDataPtr pUserData);
	bool AddClientsDisconnectionNotificationCallback(f_ClientDisconnectionNotification pf_ClientDisconnectionNotification, NotificationCallbackUserDataPtr pUserData);

	static bool StrIpToInteger(char* szIP, int* pIP);
	static bool IntegerIpToStr(int iIP, char* szIP);
};