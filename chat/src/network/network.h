using f_ClientConnectionNotification = bool(*)(bool bIsPreConnectionStep, int iConnectionCount, int iIp, char* szIP, int iPort);
using f_ClientDisconnectionNotification = bool(*)(int iConnectionCount);

struct client_receive_data_thread
{
	SOCKET m_ConnectionSocket;
	HANDLE m_ThreadHandle;
	int m_iThreadId;
	SOCKADDR_IN m_SockAddrIn;
};

class CNetwork
{
private:
	int m_iSockAddrInLength = sizeof(SOCKADDR_IN);

	bool m_bIsInitialized;
	bool m_bIsHost;
	int m_iMaxProcessedUsersNumber;
	char* m_pszIP;
	int m_IPort;
	WSADATA m_WSAdata;
	SOCKADDR_IN m_SockAddrIn;
	SOCKET m_Socket;
	std::map<int, client_receive_data_thread> m_ClientsList;
	int m_iConnectionCount;
	std::vector<net_packet> m_PacketsList;
	HANDLE m_hThreadConnectionsHost;
	bool m_bServerWasDowned;
	f_ClientConnectionNotification m_pf_ClientConnectionNotificationCallback;
	f_ClientDisconnectionNotification m_pf_ClientDisconnectionNotificationCallback;
	std::mutex m_mtxExchangePacketsData;

	bool InitializeAsHost();
	bool InitializeAsClient();
	void PacketsListCriticalSectionLock();
	void PacketsListCriticalSectionUnlock();
	void AddToPacketList(net_packet NetPacket);
	bool InvokeClientConnectionNotification(bool bIsPreConnectionStep, int iConnectionCount, int iIP, char* szIP, int iPort);
	bool InvokeClientDisconnectionNotification(int iConnectionCount);
	bool SendToSocket(SOCKET Socket, void* pPacket, int iSize);
	bool ReceivePacket(net_packet* pPacket);
	void DropConnections();
	void DisconnectSocket(SOCKET Socket);
	void DisconnectClient(client_receive_data_thread* Client);
	int GetPacketInfoLength();
public:
	CNetwork(bool IsHost, char* pszIP, int iPort, int iMaxProcessedUsersNumber);
	~CNetwork();
	
	bool Startup();
	bool SendPacketAll(void* pPacket, int iSize);
	bool SendPacketExcludeID(void* pPacket, int iSize, std::vector<int>* vIDList);
	bool SendPacketIncludeID(void* pPacket, int iSize, std::vector<int>* vIDList);
	bool GetReceivedData(net_packet* pPacket);
	bool ServerWasDowned();
	bool IsHost();
	int GetConnectedUsersCount();
	void DisconnectUser(int IdCount);
	bool GetIpByClientId(int IdCount, int* pIP);
	char* GetStrIpFromSockAddrIn(PSOCKADDR_IN pSockAddrIn);
	int GetIntegerIpFromSockAddrIn(PSOCKADDR_IN pSockAddrIn);
	int GetPortFromSockAddrIn(PSOCKADDR_IN pSockAddrIn);
	bool AddClientsConnectionNotificationCallback(f_ClientConnectionNotification pf_NewClientsNotificationCallback);
	bool AddClientsDisconnectionNotificationCallback(f_ClientDisconnectionNotification pf_ClientDisconnectionNotification);

	static bool StrIpToInteger(char* szIP, int* pIP);
	static bool IntegerIpToStr(int iIP, char* szIP);
};