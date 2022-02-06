#include "../includes.h"

CNetwork::CNetwork(bool IsHost, char* pszIP, int iPort, int iMaxProcessedUsersNumber) :
	m_bIsInitialized(false),
	m_bIsHost(IsHost),
	m_iMaxProcessedUsersNumber(iMaxProcessedUsersNumber),
	m_pszIP(pszIP),
	m_IPort(iPort),
	m_pSockAddrIn(new SOCKADDR_IN),
	m_Socket(0),
	m_bServerWasDowned(false),
	m_iConnectionCount(0),
	m_hThreadConnectionsHost(0)
{
	printf("[+] %s -> Contructor called\n", __FUNCTION__);

	memset(&this->m_WSAdata, 0, sizeof(WSADATA));
}

CNetwork::~CNetwork()
{
	printf("[+] %s -> Destructor called\n", __FUNCTION__);

	closesocket(this->m_Socket);
	DropConnections();
	WSACleanup();
}

bool CNetwork::SendPacket(void* pPacket, int iSize, SOCKET IgonreSocket)
{
	if (!this->m_bIsInitialized)
		return false;

	for (auto it = this->m_ClientsList.begin(); it != this->m_ClientsList.end(); it++)
	{
		auto& CurrentMap = *it;
		auto& Socket = CurrentMap.second.m_ConnectionSocket;

		if (!Socket)
			continue;

		if (IgonreSocket && IgonreSocket == Socket)
			continue;

		if (send(Socket, (const char*)&iSize, sizeof(int), NULL) == SOCKET_ERROR)
			continue;

		send(Socket, (const char*)pPacket, iSize, NULL);
	}

	return true;
}

bool CNetwork::SendPacket(void* pPacket, int iSize)
{
	return SendPacket(pPacket, iSize, 0);
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

	this->m_pSockAddrIn->sin_addr.S_un.S_addr = inet_addr(this->m_pszIP);
	this->m_pSockAddrIn->sin_port = htons(this->m_IPort);
	this->m_pSockAddrIn->sin_family = AF_INET;

	this->m_Socket = socket(AF_INET, SOCK_STREAM, NULL);

	if (this->m_bIsHost)
		this->m_bIsInitialized = InitializeAsHost();
	else
		this->m_bIsInitialized = InitializeAsClient();

	delete m_pSockAddrIn;

	return this->m_bIsInitialized;
}

bool CNetwork::InitializeAsHost()
{
	if (bind(this->m_Socket, (sockaddr*)this->m_pSockAddrIn, this->m_iSockAddrInLength) == SOCKET_ERROR)
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
			auto Connection = accept(_this->m_Socket, (sockaddr*)_this->m_pSockAddrIn, &_this->m_iSockAddrInLength);

			if (Connection == INVALID_SOCKET)
			{
				auto LastError = WSAGetLastError();

				if (LastError == WSAEWOULDBLOCK ||
					LastError == WSAEOPNOTSUPP ||
					LastError == WSAECONNRESET)
				{

					printf("[-] %s -> Failed to client connection at: %d, Error code: %d\n", "CNetwork::InitializeAsHost(Connection handler lambda)", (int)_this->m_ClientsList.size(), LastError);
					continue;
				}

				printf("[+] %s -> Shutdown client connection handler thread WSAErrorcode: %d\n", "CNetwork::InitializeAsHost(Connection handler lambda)", LastError);
				break;
			}

			printf("[+] %s -> Connected clientid: %d\n", "CNetwork::InitializeAsHost(Connection handler lambda)", (int)_this->m_ClientsList.size());

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

				while (true)
				{
					int DataSize = 0;

					auto recv_ret = recv(Client->m_ConnectionSocket, (char*)&DataSize, sizeof(int), NULL);

					if (recv_ret > 0)
					{
						if (DataSize)
						{
							printf("[+] %s ->  Received data at clientid: %d, data size: %d\n", "CNetwork::InitializeAsHost(Data receiver lambda)", Client->m_iThreadId, DataSize);
							auto pData = new char[DataSize];
							recv(Client->m_ConnectionSocket, (char*)pData, DataSize, NULL);
							net_packet* NetPacket = new net_packet(pData, DataSize);
							_this->SendPacket(NetPacket->m_pPacket, NetPacket->m_iSize, Client->m_ConnectionSocket);
							_this->m_PacketsList.push_back(NetPacket);
						}
					}
					else
					{
						printf("[+] %s -> Dropped connection at clientid: %d\n", "CNetwork::InitializeAsHost(Data receiver lambda)", Client->m_iThreadId);
						_this->DisconnectClient(Client);
						break;
					}
				}

				printf("[+] %s -> Exit receive thread clientid: %d\n", "CNetwork::InitializeAsHost(Data receiver lambda)", Client->m_iThreadId);

				return 0;
			};

			auto& Client = _this->m_ClientsList[_this->m_iConnectionCount];
			Client.m_ConnectionSocket = Connection;
			host_receive_thread_arg	HostReceiveThreadArg{};
			HostReceiveThreadArg.m_Network = _this;
			HostReceiveThreadArg.m_CurrentClient = &Client;
			Client.m_ThreadHandle = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &HostReceiveThreadArg, 0, nullptr);
			Client.m_iThreadId = _this->m_iConnectionCount;

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
	auto Connection = connect(this->m_Socket, (sockaddr*)this->m_pSockAddrIn, this->m_iSockAddrInLength);

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
					printf("[+] %s -> Received data from host, size: %d\n", "CNetwork::InitializeAsClient(Data receiver lambda)", DataSize);
					auto pData = new char[DataSize];
					recv(Client->m_ConnectionSocket, (char*)pData, DataSize, NULL);
					net_packet* NetPacket = new net_packet(pData, DataSize);
					_this->m_PacketsList.push_back(NetPacket);
				}
			}
			else
			{
				printf("[+] %s -> Host was closed\n", "CNetwork::InitializeAsClient(Data receiver lambda)");
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

bool CNetwork::GetReceivedData(net_packet* pPacket)
{
	return ReceivePacket(pPacket);
}

bool CNetwork::ServerWasDowned()
{
	if (this->m_bIsHost)
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

void CNetwork::DisconnectClient(client_receive_data_thread* Client)
{
	closesocket(Client->m_ConnectionSocket);
	Client->m_ConnectionSocket = 0;
}

void CNetwork::DisconnectUser(int IdCount)
{
	auto Client = &this->m_ClientsList[IdCount];
	shutdown(Client->m_ConnectionSocket, SD_BOTH);
	DisconnectClient(Client);
}