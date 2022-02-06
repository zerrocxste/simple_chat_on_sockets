struct client_receive_data_thread
{
	SOCKET m_ConnectionSocket;
	HANDLE m_ThreadHandle;
	int m_iThreadId;
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
	PSOCKADDR_IN m_pSockAddrIn;
	SOCKET m_Socket;
	std::map<int, client_receive_data_thread> m_ClientsList;
	int m_iConnectionCount;
	std::vector<net_packet*> m_PacketsList;
	HANDLE m_hThreadConnectionsHost;
	bool m_bServerWasDowned;

	bool InitializeAsHost();
	bool InitializeAsClient();
	bool SendPacket(void* pPacket, int iSize, SOCKET IgonreSocket);
	bool ReceivePacket(net_packet* pPacket);
	void DropConnections();
	void DisconnectClient(client_receive_data_thread* Client);
public:
	CNetwork(bool IsHost, char* pszIP, int iPort, int iMaxProcessedUsersNumber);
	~CNetwork();

	bool Startup();
	bool SendPacket(void* pPacket, int iSize);
	bool GetReceivedData(net_packet* pPacket);
	bool ServerWasDowned();
	bool IsHost();
	int GetConnectedUsersCount();
	void DisconnectUser(int IdCount);
};