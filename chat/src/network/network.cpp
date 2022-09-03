#include "../includes.h"

constexpr short g_DeltaPacketMagicValue = 1337;

#define SEND(s, b, l) send(s, b, l, 0)
#define RECV(s, b, l) recv(s, b, l, 0)

#ifdef _WIN32
int CNetworkTCP::g_iCreatedLinkCount = 0;
WSADATA CNetworkTCP::g_WSAdata{};
#endif

CNetworkTCP::CNetworkTCP(bool IsHost, char* pszIP, int iPort, netconnectcount iMaxProcessedUsersNumber) :
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

CNetworkTCP::~CNetworkTCP()
{
	TRACE_FUNC("Destructor called\n");

	closesocket(this->m_Socket);
	this->m_Socket = 0;

	DropConnections();

	ShutdownNetwork();
}

void CNetworkTCP::SendToSocket(void* pPacket, int iSize, const CNetworkTCP::NETSOCK Socket)
{
	if (!Socket)
		return;

	this->m_mtxSendData.lock();

	delta_packet_t deltaPacket{ g_DeltaPacketMagicValue, iSize };

	int sendedDeltaPacketSize = 0;
	while (sendedDeltaPacketSize < CNetworkTCP::iDeltaPacketLength)
	{
		auto send_ret = SEND(Socket, (char*)(&deltaPacket) + sendedDeltaPacketSize, CNetworkTCP::iDeltaPacketLength - sendedDeltaPacketSize);

		if (send_ret == SOCKET_ERROR)
		{
			TRACE_FUNC("Error send delta packet to socket: %p sended bytes: %d\n", Socket, sendedDeltaPacketSize);
			this->m_mtxSendData.unlock();
			return;
		}

		sendedDeltaPacketSize += send_ret;
	}

	int sendedDataPacket = 0;
	while (sendedDataPacket < iSize)
	{
		auto send_ret = SEND(Socket, (char*)(pPacket)+sendedDataPacket, iSize - sendedDataPacket);

		if (send_ret == SOCKET_ERROR)
		{
			TRACE_FUNC("Error send packet data to socket: %p sended bytes: %d\n", Socket, sendedDataPacket);
			this->m_mtxSendData.unlock();
			return;
		}

		sendedDataPacket += send_ret;
	}

	this->m_mtxSendData.unlock();
}

void CNetworkTCP::SendPacket(void* pPacket, int iSize)
{
	for (auto& pair : this->m_ClientsList)
	{
		auto Socket = pair.second.m_ConnectionSocket;

		if (!Socket)
			continue;

		SendToSocket(pPacket, iSize, Socket);
	}
}

void CNetworkTCP::SendPacket(void* pPacket, int iSize, const netconnectcount iConnectionID)
{
	auto Socket = this->m_ClientsList[iConnectionID].m_ConnectionSocket;

	if (!Socket)
		return;

	SendToSocket(pPacket, iSize, Socket);
}

void CNetworkTCP::SendPacket(void* pPacket, int iSize, void* pUserData, bool(*pfSortingDelegate)(void* pUserData, netconnectcount iConnectionID))
{
	if (!pfSortingDelegate)
		return;

	for (auto& pair : this->m_ClientsList)
	{
		if (!pfSortingDelegate(pUserData, pair.second.m_iConnectionID))
			continue;

		SendToSocket(pPacket, iSize, pair.second.m_ConnectionSocket);
	}
}

bool CNetworkTCP::ReceivePacket(net_packet_t* pPacket)
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

void CNetworkTCP::DropConnections()
{
	for (auto& pair : this->m_ClientsList)
	{
		auto Socket = pair.second.m_ConnectionSocket;

		if (!Socket)
			continue;

		shutdown(Socket, SD_BOTH);
	}
}

bool CNetworkTCP::InitializeNetwork()
{
#ifdef _WIN32
	if (CNetworkTCP::g_iCreatedLinkCount++ == 0)
	{
		memset(&this->g_WSAdata, 0, sizeof(WSADATA));

		if (WSAStartup(MAKEWORD(2, 2), &CNetworkTCP::g_WSAdata) != 0)
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

bool CNetworkTCP::Startup()
{
	if (this->m_bIsInitialized)
		return true;

	if (!InitializeNetwork())
		return false;

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

void CNetworkTCP::thHostClientReceive(void* arg)
{
	auto _this = ((host_receive_thread_arg_t*)arg)->m_Network;
	auto Client = ((host_receive_thread_arg_t*)arg)->m_CurrentClient;

	delete (host_receive_thread_arg_t*)arg;

	auto Socket = Client->m_ConnectionSocket;
	auto ConnectionID = Client->m_iConnectionID;

	while (true)
	{
		delta_packet_t deltaPacket{};

		int resuidalDeltaPacketSize = 0;
		while (resuidalDeltaPacketSize < CNetworkTCP::iDeltaPacketLength)
		{
			auto recv_ret = RECV(Socket, (char*)(&deltaPacket) + resuidalDeltaPacketSize, CNetworkTCP::iDeltaPacketLength - resuidalDeltaPacketSize);

			if (recv_ret <= 0)
			{
				TRACE_FUNC("Dropped connection at ID: %d\n", ConnectionID);
				_this->InvokeClientDisconnectionNotification(ConnectionID);
				_this->DisconnectClient(Client);
				return;
			}

			resuidalDeltaPacketSize += recv_ret;
		}

		if (deltaPacket.magic != g_DeltaPacketMagicValue)
		{
			TRACE_FUNC("Bad magic value from ID: %d. deltaPacket.magic: %d\n", ConnectionID, deltaPacket.magic);
			continue;
		}

		if (deltaPacket.m_iPacketSize <= 0)
		{
			TRACE_FUNC("Bad packet size length from ID: %d. deltaPacket.m_iPacketSize: %d\n", ConnectionID, deltaPacket.m_iPacketSize);
			continue;
		}

		auto pPacketData = (void*)new (std::nothrow) char[deltaPacket.m_iPacketSize];
		if (!pPacketData)
		{
			TRACE_FUNC("Failed to allocate memory from ID: %d. deltaPacket.m_iPacketSize: %d\n", ConnectionID, deltaPacket.m_iPacketSize);
			continue;
		}

		int resuidalDataPacketSize = 0;
		while (resuidalDataPacketSize < deltaPacket.m_iPacketSize)
		{
			auto recv_ret = RECV(Socket, (char*)(pPacketData)+resuidalDataPacketSize, deltaPacket.m_iPacketSize - resuidalDataPacketSize);

			if (recv_ret <= 0)
			{
				TRACE_FUNC("Dropped connection at ID: %d\n", ConnectionID);
				_this->InvokeClientDisconnectionNotification(ConnectionID);
				_this->DisconnectClient(Client);
				return;
			}

			resuidalDataPacketSize += recv_ret;
		}

		TRACE_FUNC("Packet received from ID: %d deltaPacket.magic: %d deltaPacket.m_iPacketSize: %d pPacketData: %p\n", ConnectionID, deltaPacket.magic, deltaPacket.m_iPacketSize, pPacketData);

		_this->AddToPacketList(net_packet_t(pPacketData, deltaPacket.m_iPacketSize, ConnectionID));
	}
}

void CNetworkTCP::thHostClientsHandling(void* arg)
{
	auto _this = (CNetworkTCP*)arg;

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
			TRACE_FUNC("Aborted connection by host user ID: %d\n", (int)_this->m_ClientsList.size());
			_this->DisconnectSocket(Connection);
			continue;
		}

		auto& Client = _this->m_ClientsList[_this->m_iConnectionCount];
		Client.m_ConnectionSocket = Connection;
		Client.m_iConnectionID = _this->m_iConnectionCount;
		Client.m_SockAddrIn = SockAddrIn;

		TRACE_FUNC("Connected clientid: %d\n", (int)_this->m_ClientsList.size());

		host_receive_thread_arg_t* pHostReceiveThreadArg = new host_receive_thread_arg_t();
		pHostReceiveThreadArg->m_Network = _this;
		pHostReceiveThreadArg->m_CurrentClient = &Client;

		CNetworkTCP::ThreadCreate(&CNetworkTCP::thHostClientReceive, pHostReceiveThreadArg);

		_this->InvokeClientConnectionNotification(false, _this->m_iConnectionCount, iIP, szIP, iPort);
	}
}

bool CNetworkTCP::InitializeAsHost()
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

	CNetworkTCP::ThreadCreate(&CNetworkTCP::thHostClientsHandling, this);

	return true;
}

void CNetworkTCP::thClientHostReceive(void* arg)
{
	auto _this = (CNetworkTCP*)arg;

	auto Socket = _this->m_ClientsList[CLIENT_SOCKET].m_ConnectionSocket;

	while (true)
	{
		delta_packet_t deltaPacket{};

		int resuidalDeltaPacketSize = 0;
		while (resuidalDeltaPacketSize < CNetworkTCP::iDeltaPacketLength)
		{
			auto recv_ret = RECV(Socket, (char*)(&deltaPacket) + resuidalDeltaPacketSize, CNetworkTCP::iDeltaPacketLength - resuidalDeltaPacketSize);

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
			TRACE_FUNC("Bad magic value. deltaPacket.magic: %d\n", deltaPacket.magic);
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
			auto recv_ret = RECV(Socket, (char*)(pPacketData)+resuidalDataPacketSize, deltaPacket.m_iPacketSize - resuidalDataPacketSize);

			if (recv_ret <= 0)
			{
				TRACE_FUNC("Receive delta packet. Host was closed\n");
				_this->m_bServerWasDowned = true;
				return;
			}

			resuidalDataPacketSize += recv_ret;
		}

		TRACE_FUNC("Packet received. deltaPacket.magic: %d deltaPacket.m_iPacketSize: %d pPacketData: %p\n", deltaPacket.magic, deltaPacket.m_iPacketSize, pPacketData);

		_this->AddToPacketList(net_packet_t(pPacketData, deltaPacket.m_iPacketSize, 0));
	}
}

bool CNetworkTCP::InitializeAsClient()
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
	Client.m_iConnectionID = CLIENT_SOCKET;
	Client.m_SockAddrIn = sockaddr_in();

	CNetworkTCP::ThreadCreate(&CNetworkTCP::thClientHostReceive, this);

	return true;
}

void CNetworkTCP::AddToPacketList(net_packet_t NetPacket)
{
	this->m_mtxExchangePacketsData.lock();

	this->m_PacketsList.push_back(NetPacket);

	this->m_mtxExchangePacketsData.unlock();
}

bool CNetworkTCP::AddClientsConnectionNotificationCallback(f_ClientConnectionNotification pf_NewClientsNotificationCallback, NotificationCallbackUserDataPtr pUserData)
{
	if (!IsHost())
		return false;

	if (pUserData != nullptr)
		this->m_pOnClientConnectionUserData = pUserData;

	this->m_pf_ClientConnectionNotificationCallback = pf_NewClientsNotificationCallback;

	return true;
}

bool CNetworkTCP::AddClientsDisconnectionNotificationCallback(f_ClientDisconnectionNotification pf_ClientDisconnectionNotification, NotificationCallbackUserDataPtr pUserData)
{
	if (!IsHost())
		return false;

	if (pUserData != nullptr)
		this->m_pOnClientDisconnectionUserData = pUserData;

	this->m_pf_ClientDisconnectionNotificationCallback = pf_ClientDisconnectionNotification;

	return true;
}

bool CNetworkTCP::InvokeClientConnectionNotification(bool bIsPreConnectionStep, netconnectcount iConnectionCount, int iIP, char* szIP, int iPort)
{
	if (!this->m_pf_ClientConnectionNotificationCallback)
		return true;

	return m_pf_ClientConnectionNotificationCallback(bIsPreConnectionStep, iConnectionCount, iIP, szIP, iPort, this->m_pOnClientConnectionUserData);
}

bool CNetworkTCP::InvokeClientDisconnectionNotification(netconnectcount iConnectionID)
{
	if (!this->m_pf_ClientDisconnectionNotificationCallback)
		return true;

	return m_pf_ClientDisconnectionNotificationCallback(iConnectionID, this->m_pOnClientDisconnectionUserData);
}

bool CNetworkTCP::GetReceivedData(net_packet_t* pPacket)
{
	return ReceivePacket(pPacket);
}

CNetworkTCP::NETSOCK CNetworkTCP::GetSocketFromConnectionID(netconnectcount iConnectionID)
{
	return this->m_ClientsList[iConnectionID].m_ConnectionSocket;
}

bool CNetworkTCP::ServerWasDowned()
{
	if (IsHost())
		return false;

	return this->m_bServerWasDowned;
}

bool CNetworkTCP::IsHost()
{
	return this->m_bIsHost;
}

netconnectcount CNetworkTCP::GetConnectedUsersCount()
{
	netconnectcount ret = 0;

	for (auto& pair : this->m_ClientsList)
	{
		if (!pair.second.m_ConnectionSocket)
			continue;

		ret++;
	}

	return ret;
}

void CNetworkTCP::ShutdownNetwork()
{
#ifdef _WIN32
	CNetworkTCP::g_iCreatedLinkCount--;

	if (CNetworkTCP::g_iCreatedLinkCount == 0)
		WSACleanup();
#else
	//
#endif // _WIN32
}

void CNetworkTCP::DisconnectSocket(SOCKET Socket)
{
	shutdown(Socket, SD_BOTH);
	closesocket(Socket);
}

void CNetworkTCP::DisconnectClient(client_receive_data_thread_t* Client)
{
	DisconnectSocket(Client->m_ConnectionSocket);
	Client->m_ConnectionSocket = 0;
}

void CNetworkTCP::DisconnectUser(netconnectcount iConnectionID)
{
	auto Client = &this->m_ClientsList[iConnectionID];
	DisconnectClient(Client);
}

bool CNetworkTCP::GetIpByClientId(netconnectcount iConnectionID, int* pIP)
{
	if (!IsHost())
		return false;

	auto Client = &this->m_ClientsList[iConnectionID];

	if (!Client)
		return false;

	*pIP = Client->m_SockAddrIn.sin_addr.S_un.S_addr;

	return true;
}

char* CNetworkTCP::GetStrIpFromSockAddrIn(PSOCKADDR_IN pSockAddrIn)
{
	if (!pSockAddrIn)
		return nullptr;

	return inet_ntoa(pSockAddrIn->sin_addr);
}

int CNetworkTCP::GetIntegerIpFromSockAddrIn(PSOCKADDR_IN pSockAddrIn)
{
	if (!pSockAddrIn)
		return 0;

	return pSockAddrIn->sin_addr.S_un.S_addr;
}

int CNetworkTCP::GetPortFromSockAddrIn(PSOCKADDR_IN pSockAddrIn)
{
	if (!pSockAddrIn)
		return 0;

	return ntohs(pSockAddrIn->sin_port);
}

bool CNetworkTCP::StrIpToInteger(char* szIP, int* pIP)
{
	if (!szIP || !pIP)
		return false;

	auto Result = inet_addr(szIP);

	if (Result == INADDR_NONE || Result == INADDR_ANY)
		return false;

	*pIP = Result;

	return true;
}

bool CNetworkTCP::IntegerIpToStr(int iIP, char* szIP)
{
	if (iIP == 0 || !szIP)
		return false;

	std::uint8_t* pIP = (std::uint8_t*)&iIP;

	sprintf(szIP, "%d.%d.%d.%d", pIP[0], pIP[1], pIP[2], pIP[3]);

	return true;
}