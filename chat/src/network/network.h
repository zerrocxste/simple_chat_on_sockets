using netconnectcount = unsigned int;

constexpr netconnectcount CLIENT_SOCKET = 0;

struct net_packet_t
{
	net_packet_t() :
		m_pPacket(nullptr),
		m_iSize(0),
		m_iConnectionID(0)
	{

	};

	net_packet_t(void* pPacket, int iSize, netconnectcount iConnectionID) :
		m_pPacket(pPacket),
		m_iSize(iSize),
		m_iConnectionID(iConnectionID)
	{

	}

	void* m_pPacket;
	int m_iSize;
	netconnectcount m_iConnectionID;
};

class CNetworkTCP;

using NotificationCallbackUserDataPtr = void*;

using f_ClientConnectionNotification = bool(*)(bool bIsPreConnectionStep, netconnectcount iConnectionID, int iIp, char* szIP, int iPort, NotificationCallbackUserDataPtr pUserData);
using f_ClientDisconnectionNotification = bool(*)(netconnectcount iConnectionID, NotificationCallbackUserDataPtr pUserData);

class CNetworkTCP : public IError
{
public:
	using NETSOCK = SOCKET;

#ifdef _WIN32
	using NETSOCKADDR_IN = SOCKADDR_IN;
#else
	using NETSOCKADDR_IN = sockaddr_in;
#endif // _WIN32

private:
	struct delta_packet_t
	{
		short magic;
		int delta_desc;
	};

	static const int iDeltaPacketLength = sizeof(delta_packet_t);
	int m_iSockAddrInLength = sizeof(NETSOCKADDR_IN);

#ifdef _WIN32
	static int g_iCreatedLinkCount;
	static WSADATA g_WSAdata;
#endif // _WIN32

	struct connect_data_t
	{
		CNetworkTCP::NETSOCK m_ConnectionSocket;
		netconnectcount m_iConnectionID;
		std::mutex m_mtxSendSocketBlocking;
		std::chrono::system_clock::time_point m_tpSendHeartbeat, m_tpLastHeartbeat;
		CNetworkTCP::NETSOCKADDR_IN m_SockAddrIn;
	};

	struct host_receive_thread_arg_t
	{
		CNetworkTCP* m_Network;
		connect_data_t* m_CurrentClient;
	};

	template <class T> __forceinline static bool ThreadCreate(T* pfunc, void* arg) noexcept
	{
		std::thread(pfunc, arg).detach();
		return true;
	}

	static void thHostClientReceive(void* arg);
	static void thHostHeartbeat(void* arg);
	static void thHostClientsHandling(void* arg);

	static void thClientHostReceive(void* arg);

	bool m_bIsInitialized;
	bool m_bIsHost;
	netconnectcount m_iMaxProcessedUsersNumber;
	bool m_bIsNeedExit;
	char* m_pszIP;
	int m_IPort;
	NETSOCKADDR_IN m_SockAddrIn;
	NETSOCK m_Socket;
	std::mutex m_mtxHeatbeatThread;
	std::map<netconnectcount, connect_data_t> m_ClientsList;
	netconnectcount m_iConnectionCount;
	std::vector<net_packet_t> m_PacketsList;
	bool m_bServerWasDowned;
	f_ClientConnectionNotification m_pf_ClientConnectionNotificationCallback;
	NotificationCallbackUserDataPtr m_pOnClientConnectionUserData;
	f_ClientDisconnectionNotification m_pf_ClientDisconnectionNotificationCallback;
	NotificationCallbackUserDataPtr m_pOnClientDisconnectionUserData;
	std::mutex m_mtxExchangePacketsData;

	bool InitializeNetwork();
	bool InitializeAsHost();
	bool InitializeAsClient();
	void SendHeartbeat(connect_data_t& connect_data);
	void SendToSocket(char* pPacket, int iSize, connect_data_t& connect_data);
	void AddToPacketList(net_packet_t NetPacket);
	bool InvokeClientConnectionNotification(bool bIsPreConnectionStep, netconnectcount iConnectionID, int iIP, char* szIP, int iPort);
	bool InvokeClientDisconnectionNotification(netconnectcount iConnectionID);
	bool ReceivePacket(net_packet_t* pPacket);
	void ShutdownNetwork();
	void DropConnections();
	void DisconnectSocket(SOCKET Socket);
	void DisconnectClient(connect_data_t* Client);
	void NeedExit();
	bool IsNeedExit();
public:
	CNetworkTCP(bool IsHost, char* pszIP, int iPort, netconnectcount iMaxProcessedUsersNumber);
	~CNetworkTCP();

	bool Startup();
	void SendPacket(char* pPacket, int iSize);
	void SendPacketIncludeID(char* pPacket, int iSize, const netconnectcount iConnectionID);
	void SendPacketExcludeID(char* pPacket, int iSize, const netconnectcount iConnectionID);
	void SendPacketIncludeConnectionsID(char* pPacket, int iSize, const netconnectcount* pConnectionsIDs, int iSizeofConections);
	void SendPacketExcludeConnectionsID(char* pPacket, int iSize, const netconnectcount* pConnectionsIDs, int iSizeofConections);
	void SendPacket(char* pPacket, int iSize, void* pUserData, bool(*pfSortingDelegate)(void* pUserData, netconnectcount iConnectionID));
	bool GetReceivedData(net_packet_t* pPacket);
	bool ServerWasDowned();
	bool IsHost();
	netconnectcount GetConnectedUsersCount();
	void DisconnectUser(netconnectcount iConnectionID);
	bool GetIpByClientId(netconnectcount iConnectionID, int* pIP);

	static char* GetStrIpFromSockAddrIn(PSOCKADDR_IN pSockAddrIn);
	static int GetIntegerIpFromSockAddrIn(PSOCKADDR_IN pSockAddrIn);
	static std::uint16_t GetPortFromSockAddrIn(PSOCKADDR_IN pSockAddrIn);
	static std::uint16_t GetPortFromTCPIPFormat(std::uint16_t netshort);
	static std::uint16_t GetTCPIPFormatFromPort(std::uint16_t port);

	bool AddClientsConnectionNotificationCallback(f_ClientConnectionNotification pf_NewClientsNotificationCallback, NotificationCallbackUserDataPtr pUserData);
	bool AddClientsDisconnectionNotificationCallback(f_ClientDisconnectionNotification pf_ClientDisconnectionNotification, NotificationCallbackUserDataPtr pUserData);

	static std::uint32_t StrIpToInteger(char* szIP);
	static bool IntegerIpToStr(int iIP, char szOutIP[16]);

	static bool SetSockNoDelay(CNetworkTCP::NETSOCK Sock);
};