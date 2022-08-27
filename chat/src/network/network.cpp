#include "../includes.h"

constexpr short g_DeltaPacketMagicValue = 1337;

#define SEND(s, b, l) send(s, b, l, 0)
#define RECV(s, b, l) recv(s, b, l, 0)

#ifdef _WIN32
int CNetwork::g_iCreatedLinkCount = 0;
WSADATA CNetwork::g_WSAdata{};
#endif

CNetwork::CNetwork(bool IsHost, char* pszIP, int iPort, int iMaxProcessedUsersNumber) :
	m_bIsInitialized(false),
	m_bIsHost(IsHost),
	m_iMaxProcessedUsersNumber(iMaxProcessedUsersNumber),
	m_pszIP(pszIP),
	m_IPort(iPort),
	m_Socket(0),
	m_bServerWasDowned(false),
	m_iConnectionCount(0)
{
	TRACE_FUNC("Contructor called\n");

	memset(&this->m_SockAddrIn, 0, sizeof(SOCKADDR_IN));
}

CNetwork::~CNetwork()
{
	TRACE_FUNC("Destructor called\n");

	closesocket(this->m_Socket);
	this->m_Socket = 0;

	DropConnections();

	ShutdownNetwork();
}

bool CNetwork::SendToSocket(SOCKET Socket, void* pPacket, int iSize)
{
	this->m_mtxSendData.lock();

	auto ret = false;

	deltaPacket_t deltaData{ g_DeltaPacketMagicValue, iSize };

	if (SEND(Socket, (const char*)&deltaData, CNetwork::iPacketInfoLength) == CNetwork::iPacketInfoLength)
	{
		if (SEND(Socket, (const char*)pPacket, iSize) == iSize)
			ret = true;
	}

	this->m_mtxSendData.unlock();

	return ret;
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

		SendToSocket(Socket, pPacket, iSize);
	}

	return true;
}

bool CNetwork::SendPacketExcludeID(void* pPacket, int iSize, std::vector<unsigned int>* vIDList)
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
				SendToSocket(Socket, pPacket, iSize);
				break;
			}
		}
	}

	return true;
}

bool CNetwork::SendPacketIncludeID(void* pPacket, int iSize, std::vector<unsigned int>* vIDList)
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
				SendToSocket(Socket, pPacket, iSize);
				break;
			}
		}
	}

	return true;
}

bool CNetwork::ReceivePacket(net_packet* pPacket)
{
	this->m_mtxExchangePacketsData.lock();

	auto ret = false;

	if (!this->m_PacketsList.empty())
	{
		auto it = this->m_PacketsList.begin();

		*pPacket = *it;

		this->m_PacketsList.erase(it);

		ret = true;
	}

	this->m_mtxExchangePacketsData.unlock();

	return ret;
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

bool CNetwork::InitializeNetwork()
{
#ifdef _WIN32
	if (CNetwork::g_iCreatedLinkCount++ == 0)
	{
		memset(&this->g_WSAdata, 0, sizeof(WSADATA));

		if (WSAStartup(MAKEWORD(2, 2), &CNetwork::g_WSAdata) != 0)
		{
			this->SetError(__FUNCTION__ " > WSAStartup error. WSAGetLastError: %d", WSAGetLastError());
			return false;
		}
	}
#else
	//
#endif
	return true;
}

bool CNetwork::Startup()
{
	if (m_bIsInitialized)
		return true;

	if (!InitializeNetwork())
	{
#ifdef _WIN32
		this->SetError(__FUNCTION__ " > Error initialize WinSock");
#else
		this->SetError(__FUNCTION__ " > Error initialize network");
#endif // _WIN32
		return false;
	}

	this->m_SockAddrIn.sin_addr.S_un.S_addr = inet_addr(this->m_pszIP);
	this->m_SockAddrIn.sin_port = htons(this->m_IPort);
	this->m_SockAddrIn.sin_family = AF_INET;

	this->m_Socket = socket(AF_INET, SOCK_STREAM, 0);

	if (this->m_bIsHost)
		this->m_bIsInitialized = InitializeAsHost();
	else
		this->m_bIsInitialized = InitializeAsClient();

	return this->m_bIsInitialized;
}

void CNetwork::thHostClientReceive(void* arg)
{
	auto HostReceiveThreadArg = (host_receive_thread_arg*)arg;
	auto _this = HostReceiveThreadArg->m_Network;
	auto Client = HostReceiveThreadArg->m_CurrentClient;

	auto Socket = Client->m_ConnectionSocket;
	auto ConnectionID = Client->m_iThreadId;

	while (true)
	{
		deltaPacket_t deltaPacket{};

		int resuidalDeltaPacketSize = 0;
		while (resuidalDeltaPacketSize < CNetwork::iPacketInfoLength)
		{
			auto recv_ret = RECV(Client->m_ConnectionSocket, (char*)(&deltaPacket) + resuidalDeltaPacketSize, CNetwork::iPacketInfoLength - resuidalDeltaPacketSize);

			if (recv_ret <= 0)
			{
				TRACE_FUNC("Dropped connection at clientid: %d\n", ConnectionID);
				_this->InvokeClientDisconnectionNotification(ConnectionID);
				_this->DisconnectClient(Client);
				return;
			}

			resuidalDeltaPacketSize += recv_ret;
		}

		if (deltaPacket.magic != g_DeltaPacketMagicValue)
		{
			TRACE_FUNC("Bad magic value. deltaData.magic: %d\n", deltaPacket.magic);
			continue;
		}

		if (deltaPacket.m_iPacketSize <= 0)
		{
			TRACE_FUNC("Bad packet size length. deltaPacket.m_iPacketSize: %d\n", deltaPacket.m_iPacketSize);
			continue;
		}

		auto pPacketData = (void*)new (std::nothrow) char[deltaPacket.m_iPacketSize];
		if (!pPacketData)
		{
			TRACE_FUNC("Failed to allocate memory, deltaPacket.m_iPacketSize: %d\n", deltaPacket.m_iPacketSize);
			continue;
		}

		int resuidalDataPacketSize = 0;
		while (resuidalDataPacketSize < deltaPacket.m_iPacketSize)
		{
			auto recv_ret = RECV(Client->m_ConnectionSocket, (char*)(pPacketData)+resuidalDataPacketSize, deltaPacket.m_iPacketSize - resuidalDataPacketSize);

			if (recv_ret <= 0)
			{
				TRACE_FUNC("Dropped connection at clientid: %d\n", ConnectionID);
				_this->InvokeClientDisconnectionNotification(ConnectionID);
				_this->DisconnectClient(Client);
				return;
			}

			resuidalDataPacketSize += recv_ret;
		}

		TRACE_FUNC("Packet received. deltaPacket.magic: %d deltaPacket.m_iPacketSize: %d pPacketData: %p\n", deltaPacket.magic, deltaPacket.m_iPacketSize, pPacketData);

		_this->AddToPacketList(net_packet(pPacketData, deltaPacket.m_iPacketSize, ConnectionID));
	}
}

void CNetwork::thHostClientsHandling(void* arg)
{
	auto _this = (CNetwork*)arg;

	while (true)
	{
		SOCKADDR_IN SockAddrIn{};

		auto Connection = accept(_this->m_Socket, (sockaddr*)&SockAddrIn, &_this->m_iSockAddrInLength);

		if (Connection == INVALID_SOCKET)
		{
#ifdef _WIN32
			auto LastError = WSAGetLastError();

			if (LastError == WSAEWOULDBLOCK ||
				LastError == WSAEOPNOTSUPP ||
				LastError == WSAECONNRESET)
			{
				TRACE_FUNC("Failed to client connection at: %d, Error code: %d\n", (int)_this->m_ClientsList.size(), LastError);
				continue;
			}

			TRACE_FUNC("Shutdown client connection handler thread WSAErrorcode: %d\n", LastError);
			break;
#else
			if (!_this->m_Socket)
			{
				TRACE_FUNC("Shutdown client connection handler thread\n");
				break;
			}

			TRACE_FUNC("Failed to client connection at: %d\n", (int)_this->m_ClientsList.size());
			continue;
#endif // _WIN32
		}

		_this->m_iConnectionCount++;

		auto iIP = _this->GetIntegerIpFromSockAddrIn(&SockAddrIn);
		auto szIP = _this->GetStrIpFromSockAddrIn(&SockAddrIn);
		auto iPort = _this->GetPortFromSockAddrIn(&SockAddrIn);

		TRACE_FUNC("Await new connection: %s:%d\n", szIP, iPort);

		if (!_this->InvokeClientConnectionNotification(true, _this->m_iConnectionCount, iIP, szIP, iPort))
		{
			TRACE_FUNC("Aborted connection by host user clientid: %d\n", (int)_this->m_ClientsList.size());
			_this->DisconnectSocket(Connection);
			continue;
		}

		auto& Client = _this->m_ClientsList[_this->m_iConnectionCount];
		Client.m_ConnectionSocket = Connection;
		Client.m_iThreadId = _this->m_iConnectionCount;
		Client.m_SockAddrIn = SockAddrIn;

		TRACE_FUNC("Connected clientid: %d\n", (int)_this->m_ClientsList.size());

		host_receive_thread_arg* pHostReceiveThreadArg = new host_receive_thread_arg();
		pHostReceiveThreadArg->m_Network = _this;
		pHostReceiveThreadArg->m_CurrentClient = &Client;

		CNetwork::ThreadCreate(&CNetwork::thHostClientReceive, pHostReceiveThreadArg);

		_this->InvokeClientConnectionNotification(false, _this->m_iConnectionCount, iIP, szIP, iPort);
	}
}

bool CNetwork::InitializeAsHost()
{
	if (bind(this->m_Socket, (sockaddr*)&this->m_SockAddrIn, this->m_iSockAddrInLength) == SOCKET_ERROR)
	{
#ifdef _WIN32
		this->SetError(__FUNCTION__ " > Socket bind failed. WSAGetLastError: %d", WSAGetLastError());
#else
		this->SetError(__FUNCTION__ " > Socket bind failed");
#endif // _WIN32
		return false;
	}

	if (listen(this->m_Socket, SOMAXCONN) == SOCKET_ERROR)
	{
#ifdef _WIN32
		this->SetError(__FUNCTION__ " > Listen failed. WSAGetLastError: %d", WSAGetLastError());
#else
		this->SetError(__FUNCTION__ " > Listen failed");
#endif // _WIN32
		return false;
	}

	CNetwork::ThreadCreate(&CNetwork::thHostClientsHandling, this);

	return true;
}

void CNetwork::thClientHostReceive(void* arg)
{
	auto _this = (CNetwork*)arg;
	auto Client = &_this->m_ClientsList[CLIENT_SOCKET];

	while (true)
	{
		deltaPacket_t deltaPacket{};

		int resuidalDeltaPacketSize = 0;
		while (resuidalDeltaPacketSize < CNetwork::iPacketInfoLength)
		{
			auto recv_ret = RECV(Client->m_ConnectionSocket, (char*)(&deltaPacket) + resuidalDeltaPacketSize, CNetwork::iPacketInfoLength - resuidalDeltaPacketSize);

			if (recv_ret <= 0)
			{
				TRACE_FUNC("Receive delta packet. Host was closed\n");
				_this->m_bServerWasDowned = true;
				return;
			}

			resuidalDeltaPacketSize += recv_ret;
		}

		if (deltaPacket.magic != g_DeltaPacketMagicValue)
		{
			TRACE_FUNC("Bad magic value. deltaData.magic: %d\n", deltaPacket.magic);
			continue;
		}

		if (deltaPacket.m_iPacketSize <= 0)
		{
			TRACE_FUNC("Bad packet size length. deltaPacket.m_iPacketSize: %d\n", deltaPacket.m_iPacketSize);
			continue;
		}

		auto pPacketData = (void*)new (std::nothrow) char[deltaPacket.m_iPacketSize];
		if (!pPacketData)
		{
			TRACE_FUNC("Failed to allocate memory, deltaPacket.m_iPacketSize: %d\n", deltaPacket.m_iPacketSize);
			continue;
		}

		int resuidalDataPacketSize = 0;
		while (resuidalDataPacketSize < deltaPacket.m_iPacketSize)
		{
			auto recv_ret = RECV(Client->m_ConnectionSocket, (char*)(pPacketData)+resuidalDataPacketSize, deltaPacket.m_iPacketSize - resuidalDataPacketSize);

			if (recv_ret <= 0)
			{
				TRACE_FUNC("Receive delta packet. Host was closed\n");
				_this->m_bServerWasDowned = true;
				return;
			}

			resuidalDataPacketSize += recv_ret;
		}

		TRACE_FUNC("Packet received. deltaPacket.magic: %d deltaPacket.m_iPacketSize: %d pPacketData: %p\n", deltaPacket.magic, deltaPacket.m_iPacketSize, pPacketData);

		_this->AddToPacketList(net_packet(pPacketData, deltaPacket.m_iPacketSize, 0));
	}
}

bool CNetwork::InitializeAsClient()
{
	auto Connection = connect(this->m_Socket, (sockaddr*)&this->m_SockAddrIn, this->m_iSockAddrInLength);

	if (Connection != 0)
	{
#ifdef _WIN32
		this->SetError(__FUNCTION__ " > Connection to host failed. WSAGetLastError: %d", WSAGetLastError());
#else
		this->SetError(__FUNCTION__ " > Connection to host failed");
#endif // _WIN32
		return false;
	}

	auto& Client = this->m_ClientsList[CLIENT_SOCKET];
	Client.m_ConnectionSocket = this->m_Socket;
	Client.m_iThreadId = CLIENT_SOCKET;
	Client.m_SockAddrIn = sockaddr_in();

	CNetwork::ThreadCreate(&CNetwork::thClientHostReceive, this);

	return true;
}

void CNetwork::AddToPacketList(net_packet NetPacket)
{
	this->m_mtxExchangePacketsData.lock();

	this->m_PacketsList.push_back(NetPacket);

	this->m_mtxExchangePacketsData.unlock();
}

bool CNetwork::AddClientsConnectionNotificationCallback(f_ClientConnectionNotification pf_NewClientsNotificationCallback, NotificationCallbackUserDataPtr pUserData)
{
	if (!IsHost())
		return false;

	if (pUserData != nullptr)
		this->m_pOnClientConnectionUserData = pUserData;

	this->m_pf_ClientConnectionNotificationCallback = pf_NewClientsNotificationCallback;

	return true;
}

bool CNetwork::AddClientsDisconnectionNotificationCallback(f_ClientDisconnectionNotification pf_ClientDisconnectionNotification, NotificationCallbackUserDataPtr pUserData)
{
	if (!IsHost())
		return false;

	if (pUserData != nullptr)
		this->m_pOnClientDisconnectionUserData = pUserData;

	this->m_pf_ClientDisconnectionNotificationCallback = pf_ClientDisconnectionNotification;

	return true;
}

bool CNetwork::InvokeClientConnectionNotification(bool bIsPreConnectionStep, int iConnectionCount, int iIP, char* szIP, int iPort)
{
	if (!this->m_pf_ClientConnectionNotificationCallback)
		return true;

	return m_pf_ClientConnectionNotificationCallback(bIsPreConnectionStep, iConnectionCount, iIP, szIP, iPort, this->m_pOnClientConnectionUserData);
}

bool CNetwork::InvokeClientDisconnectionNotification(int iConnectionCount)
{
	if (!this->m_pf_ClientDisconnectionNotificationCallback)
		return true;

	return m_pf_ClientDisconnectionNotificationCallback(iConnectionCount, this->m_pOnClientDisconnectionUserData);
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

unsigned int CNetwork::GetConnectedUsersCount()
{
	unsigned int ret = 0;

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

void CNetwork::ShutdownNetwork()
{
#ifdef _WIN32
	CNetwork::g_iCreatedLinkCount--;

	if (CNetwork::g_iCreatedLinkCount == 0)
		WSACleanup();
#else
	//
#endif // _WIN32
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

	sprintf(szIP, "%d.%d.%d.%d", pIP[0], pIP[1], pIP[2], pIP[3]);

	return true;
}