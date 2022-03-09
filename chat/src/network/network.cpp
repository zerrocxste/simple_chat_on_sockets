#include "../includes.h"

CNetwork::CNetwork(bool IsHost, char* pszIP, int iPort, int iMaxProcessedUsersNumber) :
	m_bIsInitialized(false),
	m_bIsHost(IsHost),
	m_iMaxProcessedUsersNumber(iMaxProcessedUsersNumber),
	m_pszIP(pszIP),
	m_IPort(iPort),
	m_Socket(0),
	m_bServerWasDowned(false),
	m_iConnectionCount(0),
	m_hThreadConnectionsHost(0)
{
	printf("[+] %s -> Contructor called\n", __FUNCTION__);

	memset(&this->m_WSAdata, 0, sizeof(WSADATA));
	memset(&this->m_SockAddrIn, 0, sizeof(SOCKADDR_IN));
}

CNetwork::~CNetwork()
{
	printf("[+] %s -> Destructor called\n", __FUNCTION__);

	closesocket(this->m_Socket);
	DropConnections();
	WSACleanup();
}

bool CNetwork::SendToSocket(SOCKET Socket, void* pPacket, int iSize)
{
	if (send(Socket, (const char*)&iSize, sizeof(int), NULL) != SOCKET_ERROR)
	{
		if (send(Socket, (const char*)pPacket, iSize, NULL) != SOCKET_ERROR)
			return true;
	}
	return false;
}

bool CNetwork::SendPacketAll(void* pPacket, int iSize)
{
	if (!this->m_bIsInitialized)
		return false;

	for (auto it = this->m_ClientsList.begin(); it != this->m_ClientsList.end(); it++)
	{
		auto& CurrentMap = *it;
		auto& Data = CurrentMap.second;
		auto& Socket = Data.m_ConnectionSocket;
		auto& CurrentID = Data.m_iThreadId;

		if (!Socket)
			continue;

		auto Status = SendToSocket(Socket, pPacket, iSize);
	}

	return true;
}

bool CNetwork::SendPacketExcludeID(void* pPacket, int iSize, std::vector<int>* vIDList)
{
	if (!this->m_bIsInitialized)
		return false;

	if (!vIDList)
		return false;

	if (vIDList->empty())
		return false;

	for (auto it = this->m_ClientsList.begin(); it != this->m_ClientsList.end(); it++)
	{
		auto& CurrentMap = *it;
		auto& Data = CurrentMap.second;
		auto& Socket = Data.m_ConnectionSocket;
		auto& CurrentID = Data.m_iThreadId;

		for (auto& ID : *vIDList)
		{
			if (CurrentID != ID)
			{
				auto Status = SendToSocket(Socket, pPacket, iSize);
				break;
			}
		}
	}

	return true;
}

bool CNetwork::SendPacketIncludeID(void* pPacket, int iSize, std::vector<int>* vIDList)
{
	if (!this->m_bIsInitialized)
		return false;

	if (!vIDList)
		return false;

	if (vIDList->empty())
		return false;

	for (auto it = this->m_ClientsList.begin(); it != this->m_ClientsList.end(); it++)
	{
		auto& CurrentMap = *it;
		auto& Data = CurrentMap.second;
		auto& Socket = Data.m_ConnectionSocket;
		auto& CurrentID = Data.m_iThreadId;

		for (auto& ID : *vIDList)
		{
			if (CurrentID == ID)
			{
				auto Status = SendToSocket(Socket, pPacket, iSize);
				break;
			}
		}
	}

	return true;
}

bool CNetwork::ReceivePacket(net_packet* pPacket)
{
	if (!this->m_PacketsList.size())
		return false;

	auto it = this->m_PacketsList.begin();
	auto NetPacket = *it;

	if (!NetPacket)
		return false;

	*pPacket = *NetPacket;
	this->m_PacketsList.erase(it);

	return true;
}

void CNetwork::DropConnections()
{
	for (auto it = this->m_ClientsList.begin(); it != this->m_ClientsList.end(); it++)
	{
		auto& CurrentMap = *it;
		auto& Client = CurrentMap.second;

		if (!Client.m_ConnectionSocket)
			continue;

		shutdown(Client.m_ConnectionSocket, SD_BOTH);
	}
}

bool CNetwork::Startup()
{
	if (m_bIsInitialized)
		return true;

	if (WSAStartup(MAKEWORD(2, 1), &this->m_WSAdata) != 0)
	{
		MessageBox(NULL, "WSAStartup error", "", MB_OK | MB_ICONERROR);
		return false;
	}

	this->m_SockAddrIn.sin_addr.S_un.S_addr = inet_addr(this->m_pszIP);
	this->m_SockAddrIn.sin_port = htons(this->m_IPort);
	this->m_SockAddrIn.sin_family = AF_INET;

	this->m_Socket = socket(AF_INET, SOCK_STREAM, NULL);

	if (this->m_bIsHost)
		this->m_bIsInitialized = InitializeAsHost();
	else
		this->m_bIsInitialized = InitializeAsClient();

	return this->m_bIsInitialized;
}

bool CNetwork::InitializeAsHost()
{
	if (bind(this->m_Socket, (sockaddr*)&this->m_SockAddrIn, this->m_iSockAddrInLength) == SOCKET_ERROR)
	{
		MessageBox(NULL, "Socket bind failed", "", MB_OK | MB_ICONERROR);
		return false;
	}

	if (listen(this->m_Socket, SOMAXCONN) == SOCKET_ERROR)
	{
		MessageBox(NULL, "Socket listen error", "", MB_OK | MB_ICONERROR);
		return false;
	}

	this->m_hThreadConnectionsHost = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)[](void* arg) -> DWORD {

		auto _this = (CNetwork*)arg;

		while (true)
		{
			SOCKADDR_IN SockAddrIn{};

			auto Connection = accept(_this->m_Socket, (sockaddr*)&SockAddrIn, &_this->m_iSockAddrInLength);

			if (Connection == INVALID_SOCKET)
			{
				auto LastError = WSAGetLastError();

				if (LastError == WSAEWOULDBLOCK ||
					LastError == WSAEOPNOTSUPP ||
					LastError == WSAECONNRESET)
				{
					printf("[-] %s -> Failed to client connection at: %d, Error code: %d\n", "CNetwork::ConnectionHandler", (int)_this->m_ClientsList.size(), LastError);
					continue;
				}

				printf("[+] %s -> Shutdown client connection handler thread WSAErrorcode: %d\n", "CNetwork::ConnectionHandler", LastError);
				break;
			}

			auto iIP = _this->GetIntegerIpFromSockAddrIn(&SockAddrIn);
			auto szIP = _this->GetStrIpFromSockAddrIn(&SockAddrIn);
			auto iPort = _this->GetPortFromSockAddrIn(&SockAddrIn);

			printf("[+] %s -> Await new connection: %s:%d\n", "CNetwork::ConnectionHandler", szIP, iPort);

			if (!_this->InvokeClientConnectionNotification(true, _this->m_iConnectionCount, iIP, szIP, iPort))
			{
				printf("[+] %s -> Aborted connection by host user clientid: %d\n", "CNetwork::ConnectionHandler", (int)_this->m_ClientsList.size());
				_this->DisconnectSocket(Connection);
				continue;
			}

			printf("[+] %s -> Connected clientid: %d\n", "CNetwork::ConnectionHandler", (int)_this->m_ClientsList.size());

			struct host_receive_thread_arg
			{
				CNetwork* m_Network;
				client_receive_data_thread* m_CurrentClient;
			};

			auto ReceiveThread = [](void* arg) -> DWORD
			{
				auto HostReceiveThreadArg = (host_receive_thread_arg*)arg;
				auto _this = HostReceiveThreadArg->m_Network;
				auto Client = HostReceiveThreadArg->m_CurrentClient;

				auto ConnectionID = Client->m_iThreadId;

				while (true)
				{
					int DataSize = 0;

					auto recv_ret = recv(Client->m_ConnectionSocket, (char*)&DataSize, sizeof(int), NULL);

					if (recv_ret > 0)
					{
						if (DataSize)
						{
							printf("[+] %s -> Received data at clientid: %d, data size: %d\n", "CNetwork::DataReceiver", ConnectionID, DataSize);
							auto pData = new char[DataSize];
							recv(Client->m_ConnectionSocket, (char*)pData, DataSize, NULL);
							net_packet* NetPacket = new net_packet(pData, DataSize, ConnectionID);
							_this->m_PacketsList.push_back(NetPacket);
						}
					}
					else
					{
						printf("[+] %s -> Dropped connection at clientid: %d\n", "CNetwork::DataReceiver", ConnectionID);
						_this->InvokeClientDisconnectionNotification(ConnectionID);
						_this->DisconnectClient(Client);
						break;
					}
				}

				printf("[+] %s -> Exit receive thread clientid: %d\n", "CNetwork::DataReceiver", ConnectionID);

				return 0;
			};

			auto& Client = _this->m_ClientsList[_this->m_iConnectionCount];
			Client.m_ConnectionSocket = Connection;
			host_receive_thread_arg	HostReceiveThreadArg{};
			HostReceiveThreadArg.m_Network = _this;
			HostReceiveThreadArg.m_CurrentClient = &Client;
			Client.m_ThreadHandle = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &HostReceiveThreadArg, 0, nullptr);
			Client.m_iThreadId = _this->m_iConnectionCount;
			Client.m_SockAddrIn = SockAddrIn;

			_this->InvokeClientConnectionNotification(false, _this->m_iConnectionCount, iIP, szIP, iPort);

			_this->m_iConnectionCount++;
		}

		return 0;

		}, this, 0, nullptr);

	if (!m_hThreadConnectionsHost)
		return false;

	return true;
}

bool CNetwork::InitializeAsClient()
{
	auto Connection = connect(this->m_Socket, (sockaddr*)&this->m_SockAddrIn, this->m_iSockAddrInLength);

	if (Connection != 0)
	{
		MessageBox(NULL, "Connection to host failed", "", MB_OK | MB_ICONERROR);
		return false;
	}

	auto ReceiveThread = [](void* arg) -> DWORD
	{
		auto _this = (CNetwork*)arg;
		auto Client = &_this->m_ClientsList[CLIENT_SOCKET];

		while (true)
		{
			int DataSize = 0;

			auto recv_ret = recv(Client->m_ConnectionSocket, (char*)&DataSize, sizeof(int), NULL);

			if (recv_ret > 0)
			{
				if (DataSize)
				{
					printf("[+] %s -> Received data from host, size: %d\n", "CNetwork::DataReceiver", DataSize);
					auto pData = new char[DataSize];
					recv(Client->m_ConnectionSocket, (char*)pData, DataSize, NULL);
					net_packet* NetPacket = new net_packet(pData, DataSize, 0);
					_this->m_PacketsList.push_back(NetPacket);
				}
			}
			else
			{
				printf("[+] %s -> Host was closed\n", "CNetwork::DataReceiver");
				_this->m_bServerWasDowned = true;
				break;
			}
		}

		return 0;
	};

	auto& Client = this->m_ClientsList[CLIENT_SOCKET];
	Client.m_ConnectionSocket = this->m_Socket;
	Client.m_ThreadHandle = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, this, 0, nullptr);
	Client.m_iThreadId = CLIENT_SOCKET;

	return true;
}

bool CNetwork::AddClientsConnectionNotificationCallback(f_ClientConnectionNotification pf_NewClientsNotificationCallback)
{
	if (!IsHost())
		return false;

	this->m_pf_ClientConnectionNotificationCallback = pf_NewClientsNotificationCallback;

	return true;
}

bool CNetwork::AddClientsDisconnectionNotificationCallback(f_ClientDisconnectionNotification pf_ClientDisconnectionNotification)
{
	if (!IsHost())
		return false;

	this->m_pf_ClientDisconnectionNotificationCallback = pf_ClientDisconnectionNotification;

	return true;
}

bool CNetwork::InvokeClientConnectionNotification(bool bIsPreConnectionStep, int iConnectionCount, int iIP, char* szIP, int iPort)
{
	if (!this->m_pf_ClientConnectionNotificationCallback)
		return true;

	return m_pf_ClientConnectionNotificationCallback(bIsPreConnectionStep, iConnectionCount, iIP, szIP, iPort);
}

bool CNetwork::InvokeClientDisconnectionNotification(int iConnectionCount)
{
	if (!this->m_pf_ClientDisconnectionNotificationCallback)
		return true;

	return m_pf_ClientDisconnectionNotificationCallback(iConnectionCount);
}

bool CNetwork::GetReceivedData(net_packet* pPacket)
{
	return ReceivePacket(pPacket);
}

bool CNetwork::ServerWasDowned()
{
	if (IsHost())
		return false;

	return this->m_bServerWasDowned;
}

bool CNetwork::IsHost()
{
	return this->m_bIsHost;
}

int CNetwork::GetConnectedUsersCount()
{
	auto ret = 0;

	for (auto it = this->m_ClientsList.begin(); it != this->m_ClientsList.end(); it++)
	{
		auto& CurrentMap = *it;
		auto& Socket = CurrentMap.second.m_ConnectionSocket;

		if (!Socket)
			continue;

		ret++;
	}

	return ret;
}

void CNetwork::DisconnectSocket(SOCKET Socket)
{
	shutdown(Socket, SD_BOTH);
	closesocket(Socket);
}

void CNetwork::DisconnectClient(client_receive_data_thread* Client)
{
	DisconnectSocket(Client->m_ConnectionSocket);
	Client->m_ConnectionSocket = 0;
}

void CNetwork::DisconnectUser(int IdCount)
{
	auto Client = &this->m_ClientsList[IdCount];
	DisconnectClient(Client);
}

bool CNetwork::GetIpByClientId(int IdCount, int* pIP)
{
	if (!IsHost())
		return false;

	auto Client = &this->m_ClientsList[IdCount];

	if (!Client)
		return false;

	*pIP = Client->m_SockAddrIn.sin_addr.S_un.S_addr;

	return true;
}

char* CNetwork::GetStrIpFromSockAddrIn(PSOCKADDR_IN pSockAddrIn)
{
	if (!pSockAddrIn)
		return nullptr;

	return inet_ntoa(pSockAddrIn->sin_addr);
}

int CNetwork::GetIntegerIpFromSockAddrIn(PSOCKADDR_IN pSockAddrIn)
{
	if (!pSockAddrIn)
		return 0;

	return pSockAddrIn->sin_addr.S_un.S_addr;
}

int CNetwork::GetPortFromSockAddrIn(PSOCKADDR_IN pSockAddrIn)
{
	if (!pSockAddrIn)
		return 0;

	return ntohs(pSockAddrIn->sin_port);
}

bool CNetwork::StrIpToInteger(char* szIP, int* pIP)
{
	if (!szIP || !pIP)
		return false;

	auto Result = inet_addr(szIP);

	if (Result == INADDR_NONE || Result == INADDR_ANY)
		return false;

	*pIP = Result;

	return true;
}

bool CNetwork::IntegerIpToStr(int iIP, char* szIP)
{
	if (iIP == 0 || !szIP)
		return false;

	std::uint8_t* pIP = (std::uint8_t*)&iIP;

	sprintf(szIP, "%d.%d.%d.%d\0", pIP[0], pIP[1], pIP[2], pIP[3]);

	return true;
}