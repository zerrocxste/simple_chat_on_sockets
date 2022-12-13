#include "../includes.h"

constexpr short DELTA_PACKET_MAGIC = 1337;
constexpr auto HEARTBEAT_ACK = -1337;

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
	m_bIsNeedExit(false),
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

	NeedExit();
	this->m_mtxHeatbeatThread.lock();
	this->m_mtxExchangePacketsData.lock();

	closesocket(this->m_Socket);
	this->m_Socket = 0;

	DropConnections();

	ShutdownNetwork();

	this->m_mtxHeatbeatThread.unlock();
	this->m_mtxExchangePacketsData.unlock();
}

void CNetworkTCP::SendHeartbeat(connect_data_t& connect_data)
{
	auto Socket = connect_data.m_ConnectionSocket;

	if (Socket == 0)
		return;

	connect_data.m_mtxSendSocketBlocking.lock();

	delta_packet_t deltaPacket{ DELTA_PACKET_MAGIC, HEARTBEAT_ACK };

	auto pDeltaPacket = (char*)&deltaPacket;
	auto iDeltaPacketSize = CNetworkTCP::iDeltaPacketLength;

	while (iDeltaPacketSize > 0)
	{
		auto send_ret = SEND(Socket, pDeltaPacket, iDeltaPacketSize);

		if (send_ret == SOCKET_ERROR)
		{
			TRACE_FUNC("Error send heartbeat delta packet to socket: %p sended bytes: %d\n", Socket, CNetworkTCP::iDeltaPacketLength - iDeltaPacketSize);
			connect_data.m_mtxSendSocketBlocking.unlock();
			return;
		}

		pDeltaPacket += send_ret;
		iDeltaPacketSize -= send_ret;
	}

	connect_data.m_tpSendHeartbeat = {};
	connect_data.m_tpLastHeartbeat = std::chrono::system_clock::now();

	connect_data.m_mtxSendSocketBlocking.unlock();
}

void CNetworkTCP::SendToSocket(char* pPacket, int iSize, connect_data_t& connect_data)
{
	auto Socket = connect_data.m_ConnectionSocket;

	if (Socket == 0)
		return;

	connect_data.m_mtxSendSocketBlocking.lock();

	delta_packet_t deltaPacket{ DELTA_PACKET_MAGIC, iSize };

	auto iDeltaPacketSended = 0;
	while (iDeltaPacketSended < CNetworkTCP::iDeltaPacketLength)
	{
		auto send_ret = SEND(Socket, (char*)&deltaPacket + iDeltaPacketSended, CNetworkTCP::iDeltaPacketLength - iDeltaPacketSended);

		if (send_ret == SOCKET_ERROR)
		{
			TRACE_FUNC("Error send delta packet to socket: %p sended bytes: %d\n", Socket, iDeltaPacketSended);
			connect_data.m_mtxSendSocketBlocking.unlock();
			return;
		}

		iDeltaPacketSended += send_ret;
	}

	auto iPacketSended = 0;
	while (iPacketSended < iSize)
	{
		auto send_ret = SEND(Socket, pPacket + iPacketSended, iSize - iPacketSended);

		if (send_ret == SOCKET_ERROR)
		{
			TRACE_FUNC("Error send packet data to socket: %p sended bytes: %d\n", Socket, iPacketSended);
			connect_data.m_mtxSendSocketBlocking.unlock();
			return;
		}

		iPacketSended += send_ret;
	}

	connect_data.m_mtxSendSocketBlocking.unlock();
}

void CNetworkTCP::SendPacket(char* pPacket, int iSize)
{
	for (auto& pair : this->m_ClientsList)
	{
		SendToSocket(pPacket, iSize, pair.second);
	}
}

void CNetworkTCP::SendPacketIncludeID(char* pPacket, int iSize, const netconnectcount iConnectionID)
{
	SendToSocket(pPacket, iSize, this->m_ClientsList[iConnectionID]);
}

void CNetworkTCP::SendPacketExcludeID(char* pPacket, int iSize, const netconnectcount iConnectionID)
{
	for (auto& pair : this->m_ClientsList)
	{
		auto& data = pair.second;

		if (data.m_iConnectionID == iConnectionID)
			continue;

		SendToSocket(pPacket, iSize, data);
	}
}

void CNetworkTCP::SendPacketIncludeConnectionsID(char* pPacket, int iSize, const netconnectcount* pConnectionsIDs, int iSizeofConections)
{
	if (iSizeofConections == 0)
		return;

	for (auto& pair : this->m_ClientsList)
	{
		auto& data = pair.second;

		for (auto i = 0; i < iSizeofConections; i++)
		{
			if (data.m_iConnectionID == pConnectionsIDs[i])
			{
				SendToSocket(pPacket, iSize, data);
				break;
			}
		}
	}
}

void CNetworkTCP::SendPacketExcludeConnectionsID(char* pPacket, int iSize, const netconnectcount* pConnectionsIDs, int iSizeofConections)
{
	if (iSizeofConections == 0)
		return;

	for (auto& pair : this->m_ClientsList)
	{
		auto& data = pair.second;

		auto bFounded = false;

		for (auto i = 0; i < iSizeofConections; i++)
		{
			if (data.m_iConnectionID == pConnectionsIDs[i])
			{
				bFounded = true;
				break;
			}
		}

		if (!bFounded)
			SendToSocket(pPacket, iSize, data);
	}
}

void CNetworkTCP::SendPacket(char* pPacket, int iSize, void* pUserData, bool(*pfSortingDelegate)(void* pUserData, netconnectcount iConnectionID))
{
	if (!pfSortingDelegate)
		return;

	for (auto& pair : this->m_ClientsList)
	{
		if (!pfSortingDelegate(pUserData, pair.second.m_iConnectionID))
			continue;

		SendToSocket(pPacket, iSize, pair.second);
	}
}

bool CNetworkTCP::ReceivePacket(net_packet_t* pPacket)
{
	if (IsNeedExit())
		return false;

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

		if (Socket == 0)
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

	const int iReceiveBuffer = 1 * 1024 * 1024; //orginal: 65535
	const int iSendBuffer = 4 * 1024 * 1024; //4 megabytes
	setsockopt(this->m_Socket, SOL_SOCKET, SO_RCVBUF, (char*)&iReceiveBuffer, sizeof(int));
	setsockopt(this->m_Socket, SOL_SOCKET, SO_SNDBUF, (char*)&iSendBuffer, sizeof(int));

	SetSockNoDelay(this->m_Socket);

	return this->m_bIsInitialized = (this->m_bIsHost ? InitializeAsHost() : InitializeAsClient());
}

void CNetworkTCP::thHostClientReceive(void* arg)
{
	auto p_host_receive_thread_arg_t = (host_receive_thread_arg_t*)arg;

	auto _this = p_host_receive_thread_arg_t->m_Network;
	auto Client = p_host_receive_thread_arg_t->m_CurrentClient;

	delete p_host_receive_thread_arg_t;

	auto Socket = Client->m_ConnectionSocket;
	auto ConnectionID = Client->m_iConnectionID;

	while (!_this->IsNeedExit())
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

		if (deltaPacket.magic != DELTA_PACKET_MAGIC)
		{
			TRACE_FUNC("Bad magic value from ID: %d. deltaPacket.magic: %d\n", ConnectionID, deltaPacket.magic);
			continue;
		}

		if (deltaPacket.delta_desc == HEARTBEAT_ACK)
		{
			//TRACE_FUNC("Hearthbeat ack from id: %d\n", ConnectionID);
			Client->m_tpSendHeartbeat = std::chrono::system_clock::now();
			Client->m_tpLastHeartbeat = {};
			continue;
		}

		if (deltaPacket.delta_desc <= 0)
		{
			TRACE_FUNC("Bad packet size length from ID: %d. deltaPacket.delta_desc: %d\n", ConnectionID, deltaPacket.delta_desc);
			continue;
		}

		auto pPacketData = (void*)new (std::nothrow) char[deltaPacket.delta_desc];
		if (!pPacketData)
		{
			TRACE_FUNC("Failed to allocate memory from ID: %d. deltaPacket.delta_desc: %d\n", ConnectionID, deltaPacket.delta_desc);
			continue;
		}

		int resuidalDataPacketSize = 0;
		while (resuidalDataPacketSize < deltaPacket.delta_desc)
		{
			auto recv_ret = RECV(Socket, (char*)(pPacketData)+resuidalDataPacketSize, deltaPacket.delta_desc - resuidalDataPacketSize);

			if (recv_ret <= 0)
			{
				TRACE_FUNC("Dropped connection at ID: %d\n", ConnectionID);
				_this->InvokeClientDisconnectionNotification(ConnectionID);
				_this->DisconnectClient(Client);
				delete[] pPacketData;
				return;
			}

			resuidalDataPacketSize += recv_ret;
		}

		TRACE_FUNC("Packet received from ID: %d deltaPacket.magic: %d deltaPacket.delta_desc: %d pPacketData: %p\n", ConnectionID, deltaPacket.magic, deltaPacket.delta_desc, pPacketData);

		_this->AddToPacketList(net_packet_t(pPacketData, deltaPacket.delta_desc, ConnectionID));
	}
}

void CNetworkTCP::thHostHeartbeat(void* arg)
{
	using namespace std;

	auto _this = (CNetworkTCP*)arg;

	while (!_this->IsNeedExit())
	{
		_this->m_mtxHeatbeatThread.lock();

		for (auto& i : _this->m_ClientsList)
		{
			auto& data = i.second;

			if (data.m_ConnectionSocket == 0)
				continue;

			if (data.m_tpSendHeartbeat.time_since_epoch().count() != 0 && chrono::duration_cast<chrono::minutes>(chrono::system_clock::now() - data.m_tpSendHeartbeat).count() >= 1)
			{
				//TRACE_FUNC("Send heartbeat message to id: %d\n", data.m_iConnectionID);
				_this->SendHeartbeat(data);
			}

			if (data.m_tpLastHeartbeat.time_since_epoch().count() != 0 && chrono::duration_cast<chrono::seconds>(chrono::system_clock::now() - data.m_tpLastHeartbeat).count() >= 15)
			{
				TRACE_FUNC("Failed heartbeat for id: %d\n", data.m_iConnectionID);
				_this->InvokeClientDisconnectionNotification(data.m_iConnectionID);
				_this->DisconnectClient(&data);
				continue;
			}
		}

		this_thread::sleep_for(chrono::milliseconds(100));

		_this->m_mtxHeatbeatThread.unlock();
	}

	TRACE_FUNC("Exit");
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
		Client.m_tpSendHeartbeat = std::chrono::system_clock::now();

		TRACE_FUNC("Connected clientid: %d\n", (int)_this->m_ClientsList.size());

		auto* pHostReceiveThreadArg = new host_receive_thread_arg_t();
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
	CNetworkTCP::ThreadCreate(&CNetworkTCP::thHostHeartbeat, this);

	return true;
}

void CNetworkTCP::thClientHostReceive(void* arg)
{
	auto _this = (CNetworkTCP*)arg;

	auto& HostData = _this->m_ClientsList[CLIENT_SOCKET];

	auto Socket = HostData.m_ConnectionSocket;

	while (!_this->IsNeedExit())
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

		if (deltaPacket.magic != DELTA_PACKET_MAGIC)
		{
			TRACE_FUNC("Bad magic value. deltaPacket.magic: %d\n", deltaPacket.magic);
			continue;
		}

		if (deltaPacket.delta_desc == HEARTBEAT_ACK)
		{
			//TRACE_FUNC("Heartbeat message\n");
			_this->SendHeartbeat(HostData);
			continue;
		}

		if (deltaPacket.delta_desc <= 0)
		{
			TRACE_FUNC("Bad packet size length. deltaPacket.delta_desc: %d\n", deltaPacket.delta_desc);
			continue;
		}

		auto pPacketData = (void*)new (std::nothrow) char[deltaPacket.delta_desc];
		if (!pPacketData)
		{
			TRACE_FUNC("Failed to allocate memory, deltaPacket.delta_desc: %d\n", deltaPacket.delta_desc);
			continue;
		}

		int resuidalDataPacketSize = 0;
		while (resuidalDataPacketSize < deltaPacket.delta_desc)
		{
			auto recv_ret = RECV(Socket, (char*)(pPacketData)+resuidalDataPacketSize, deltaPacket.delta_desc - resuidalDataPacketSize);

			if (recv_ret <= 0)
			{
				TRACE_FUNC("Receive delta packet. Host was closed\n");
				delete[] pPacketData;
				_this->m_bServerWasDowned = true;
				return;
			}

			resuidalDataPacketSize += recv_ret;
		}

		TRACE_FUNC("Packet received. deltaPacket.magic: %d deltaPacket.delta_desc: %d pPacketData: %p\n", deltaPacket.magic, deltaPacket.delta_desc, pPacketData);

		_this->AddToPacketList(net_packet_t(pPacketData, deltaPacket.delta_desc, 0));
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

	return this->m_pf_ClientConnectionNotificationCallback(bIsPreConnectionStep, iConnectionCount, iIP, szIP, iPort, this->m_pOnClientConnectionUserData);
}

bool CNetworkTCP::InvokeClientDisconnectionNotification(netconnectcount iConnectionID)
{
	if (!this->m_pf_ClientDisconnectionNotificationCallback)
		return true;

	return this->m_pf_ClientDisconnectionNotificationCallback(iConnectionID, this->m_pOnClientDisconnectionUserData);
}

bool CNetworkTCP::GetReceivedData(net_packet_t* pPacket)
{
	return ReceivePacket(pPacket);
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
		if (pair.second.m_ConnectionSocket == 0)
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

void CNetworkTCP::DisconnectClient(connect_data_t* Client)
{
	DisconnectSocket(Client->m_ConnectionSocket);
	Client->m_ConnectionSocket = 0;
}

void CNetworkTCP::DisconnectUser(netconnectcount iConnectionID)
{
	auto Client = &this->m_ClientsList[iConnectionID];
	DisconnectClient(Client);
}

void CNetworkTCP::NeedExit()
{
	this->m_bIsNeedExit = true;
}

bool CNetworkTCP::IsNeedExit()
{
	return this->m_bIsNeedExit;
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

std::uint16_t CNetworkTCP::GetPortFromSockAddrIn(PSOCKADDR_IN pSockAddrIn)
{
	if (!pSockAddrIn)
		return 0;

	return ntohs(pSockAddrIn->sin_port);
}

std::uint16_t CNetworkTCP::GetPortFromTCPIPFormat(std::uint16_t netshort)
{
	return htons(netshort);
}

std::uint16_t CNetworkTCP::GetTCPIPFormatFromPort(std::uint16_t port)
{
	return ntohs(port);
}

std::uint32_t CNetworkTCP::StrIpToInteger(char* szIP)
{
	if (!szIP)
		return 0;

	auto ret = inet_addr(szIP);

	if (ret == INADDR_NONE
		|| ret == INADDR_ANY) /*MSDN: On Windows XP and earlier if the string in the cp parameter is an empty string, then inet_addr returns the value INADDR_ANY. If NULL is passed in the cp parameter, then inet_addr returns the value INADDR_NONE.*/
		return 0;

	return ret;
}

bool CNetworkTCP::IntegerIpToStr(int iIP, char szOutIP[16])
{
	if (!szOutIP)
		return false;

	std::uint8_t* pIP = (std::uint8_t*)&iIP;
	sprintf(szOutIP, "%d.%d.%d.%d", pIP[0], pIP[1], pIP[2], pIP[3]); //???????????????????????????????????????????????
	return true;
}

bool CNetworkTCP::SetSockNoDelay(CNetworkTCP::NETSOCK Sock)
{
	static int yes = 1;
	auto ret = setsockopt(Sock, IPPROTO_TCP, TCP_NODELAY, (const char*)&yes, sizeof(int));
	if (ret != 0)
		TRACE_FUNC("Error set TCP_NODELAY setsockopt ret: %d\n", ret);
	return ret == 0;
}