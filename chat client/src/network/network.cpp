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
	m_HandleThreadConnectionsHost(0)
{
	printf("[+] %s -> Contructor called\n", __FUNCTION__);

	memset(&this->m_WSAdata, 0, sizeof(WSADATA));
}

CNetwork::~CNetwork()
{
	printf("[+] %s -> Destructor called\n", __FUNCTION__);

	delete m_pSockAddrIn;
	if (this->m_HandleThreadConnectionsHost)
	{
		TerminateThread(this->m_HandleThreadConnectionsHost, 0);
		CloseHandle(this->m_HandleThreadConnectionsHost);
	}
	DropConnections();
	WSACleanup();
}

bool CNetwork::SendPacket(void* pPacket, int iSize, SOCKET IgonreSocket)
{
	if (!this->m_bIsInitialized)
		return false;

	for (auto it = this->m_Clients.begin(); it != this->m_Clients.end(); it++)
	{
		auto& CurrentMap = *it;
		auto& Socket = CurrentMap.second.m_ConnectionSocket;

		if (!Socket)
			continue;

		if (IgonreSocket && IgonreSocket == Socket)
			continue;

		if (send(Socket, (const char*)&iSize, sizeof(int), NULL) == SOCKET_ERROR)
			return false;

		if (send(Socket, (const char*)pPacket, iSize, NULL) == SOCKET_ERROR)
			return false;
	}

	return true;
}

bool CNetwork::SendPacket(void* pPacket, int iSize)
{
	return SendPacket(pPacket, iSize, 0);
}

bool CNetwork::ReceivePacket(net_packet* pPacket)
{
	for (auto it = this->m_Clients.begin(); it != this->m_Clients.end(); it++)
	{
		auto& CurrentMap = *it;
		auto& Client = CurrentMap.second;

		if (!Client.m_ConnectionSocket)
			continue;

		if (!Client.m_pNetPacket)
			continue;

		*pPacket = *Client.m_pNetPacket;
		Client.m_pNetPacket = nullptr;
		return true;
	}

	return false;
}

void CNetwork::DropConnections()
{
	for (auto it = this->m_Clients.begin(); it != this->m_Clients.end(); it++)
	{
		auto& CurrentMap = *it;
		auto& Client = CurrentMap.second;

		closesocket(Client.m_ConnectionSocket);
		TerminateThread(Client.m_ThreadHandle, 0);
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

	this->m_HandleThreadConnectionsHost = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)[](void* arg) -> DWORD {
		
		auto _this = (CNetwork*)arg;

		while (true)
		{
			auto Connection = accept(_this->m_Socket, (sockaddr*)_this->m_pSockAddrIn, &_this->m_iSockAddrInLength);

			if (Connection == 0)
			{
				printf("[-] Failed to client connection at: %d\n", _this->m_Clients.size());
				continue;
			}

			printf("[+] Connected client at: %d\n", _this->m_Clients.size());

			struct host_receive_thread_arg
			{
				CNetwork* Network;
				client_reciev_data_thread* Client;
			} HostReceiveThreadArg;

			auto ReceiveThread = [](void* arg) -> DWORD
			{
				auto HostReceiveThreadArg = (host_receive_thread_arg*)arg;
				auto _this = HostReceiveThreadArg->Network;
				auto Client = HostReceiveThreadArg->Client;

				while (true)
				{
					int DataSize = 0;

					auto recv_ret = recv(Client->m_ConnectionSocket, (char*)&DataSize, sizeof(int), NULL);

					if (recv_ret > 0 && DataSize)
					{
						auto pData = new char[DataSize];
						recv(Client->m_ConnectionSocket, (char*)pData, DataSize, NULL);
						net_packet* packet = new net_packet(pData, DataSize);
						_this->SendPacket(packet->m_pPacket, packet->m_iSize, Client->m_ConnectionSocket);
						Client->m_pNetPacket = packet;
					}
				}

				return 0;
			};

			auto& Client = _this->m_Clients[_this->m_iConnectionCount];
			Client.m_ConnectionSocket = Connection;
			HostReceiveThreadArg.Network = _this;
			HostReceiveThreadArg.Client = &Client;
			Client.m_ThreadHandle = CreateThread(nullptr, 0, (LPTHREAD_START_ROUTINE)ReceiveThread, &HostReceiveThreadArg, 0, nullptr);
			Client.m_iThreadId = _this->m_iConnectionCount;

			_this->m_iConnectionCount++;
		}

		return 0;
		
		}, this, 0, nullptr);

	if (!m_HandleThreadConnectionsHost)
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
		auto& Client = _this->m_Clients[CLIENT_SOCKET];

		while (true)
		{
			int DataSize = 0;

			auto recv_ret = recv(Client.m_ConnectionSocket, (char*)&DataSize, sizeof(int), NULL);

			if (recv_ret > 0)
			{
				if (DataSize)
				{
					auto pData = new char[DataSize];
					recv(Client.m_ConnectionSocket, (char*)pData, DataSize, NULL);
					net_packet* packet = new net_packet(pData, DataSize);
					Client.m_pNetPacket = packet;
				}
			}
			else
			{
				_this->m_bServerWasDowned = true;
			}
		}
	};

	auto& Client = this->m_Clients[CLIENT_SOCKET];
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

	for (auto it = this->m_Clients.begin(); it != this->m_Clients.end(); it++)
	{
		auto& CurrentMap = *it;
		auto& Socket = CurrentMap.second.m_ConnectionSocket;

		if (Socket)
			ret++;
	}

	return ret;
}

void CNetwork::DisconnectUser(int IdCount)
{
	auto& Client = this->m_Clients[IdCount];
	TerminateThread(Client.m_ThreadHandle, 0);
	closesocket(Client.m_ConnectionSocket);
	Client.m_ConnectionSocket = 0;
}