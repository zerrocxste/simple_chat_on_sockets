#include "../../includes.h"

chat_packet_data_t::chat_packet_data_t() :
	m_pPacket(nullptr),
	m_iProcessStep(0),
	m_iMessageLength(0)
{

}

chat_packet_data_t::chat_packet_data_t(const chat_packet_data_t& PacketData)
{
	*this = PacketData;
}

chat_packet_data_t::chat_packet_data_t(int iMessageLength)
{
	this->m_pPacket = new (std::nothrow) char[iMessageLength];
	if (!this->m_pPacket)
		std::abort();
	this->m_iProcessStep = 0;
	this->m_iMessageLength = iMessageLength;
}

chat_packet_data_t::chat_packet_data_t(char* pPacket, int iMessageLength)
{
	this->m_pPacket = pPacket;
	this->m_iProcessStep = 0;
	this->m_iMessageLength = iMessageLength;
}

chat_packet_data_t::~chat_packet_data_t()
{
	release();
}

chat_packet_data_t::operator bool() const
{
	return this->m_pPacket != nullptr;
}

bool chat_packet_data_t::operator!() const
{
	return !this->operator bool();
}

void chat_packet_data_t::operator=(const chat_packet_data_t& PacketData)
{
	this->m_pPacket = PacketData.m_pPacket;
	this->m_iProcessStep = PacketData.m_iProcessStep;
	this->m_iMessageLength = PacketData.m_iMessageLength;
	PacketData.m_pPacket = nullptr;
}

void chat_packet_data_t::reset_process_step()
{
	this->m_iProcessStep = 0;
}

void chat_packet_data_t::release()
{
	if (*this)
	{
		delete[] this->m_pPacket;
		this->m_pPacket = nullptr;
	}
}

constexpr auto SEND_CHAT_TO_HOST_MSG = "SEND_CHAT_TO_HOST_MSG";
constexpr auto SEND_CHAT_TO_CLIENT_MSG = "SEND_CHAT_TO_CLIENT_MSG";
constexpr auto ADMIN_REQUEST_MSG = "ADMIN_REQUEST_MSG";
constexpr auto ADMIN_STATUS_MSG = "ADMIN_STATUS_MSG";
constexpr auto CLIENT_CONNECTED_MSG = "CLIENT_CONNECTED_MSG";
constexpr auto CLIENT_CHAT_USER_ID = "CLIENT_CHAT_USER_ID";
constexpr auto DELETE_CHAT_MSG = "DELETE_CHAT_MSG";
constexpr auto ONLINE_LIST_MSG = "ONLINE_LIST_MSG";

constexpr auto ADMIN_GRANTED = "Admin access granted!";
constexpr auto ADMIN_NOT_GRANTED = "Login or password is incorrect";

bool CNetworkChatManager::OnConnectionNotification(bool bIsPreConnectionStep, netconnectcount iConnectionID, int iIp, char* szIP, int iPort, NotificationCallbackUserDataPtr pUserData)
{
	auto pNetworkChatManager = (CNetworkChatManager*)pUserData;

	auto pCustomChatMangerHandler = pNetworkChatManager->m_pCustomChatMangerHandler;

	if (bIsPreConnectionStep)
	{
		if (pCustomChatMangerHandler != nullptr)
		{
			if (!pCustomChatMangerHandler->OnPreConnectionStep(iConnectionID, iIp, szIP, iPort))
				return false;
		}

		TRACE_FUNC("Connected. ID: %d, %s:%d\n", iConnectionID, szIP, iPort);
		pNetworkChatManager->AddUser(iConnectionID, iIp, iPort);
	}
	else
	{
		pNetworkChatManager->SendConnectedMessage(iConnectionID);
		pNetworkChatManager->SendActiveUsersToClients();
	}

	return true;
}

bool CNetworkChatManager::OnDisconnectionNotification(netconnectcount iConnectionID, NotificationCallbackUserDataPtr pUserData)
{
	auto pNetworkChatManager = (CNetworkChatManager*)pUserData;

	TRACE_FUNC("Disconnected. ID: %d\n", iConnectionID);

	return pNetworkChatManager->DisconnectUser(iConnectionID);
}

CNetworkChatManager::CNetworkChatManager(bool IsHost, char* szUsername, char* pszIP, int iPort, netconnectcount iMaxProcessedUsersNumber) :
	m_bIsInitialized(false),
	m_bIsHost(IsHost),
	m_bIsAdmin(false),
	m_bNeedExit(false),
	m_iUsersConnectedToHost(0),
	m_iUsersConnectedToHostPrevFrame(0),
	m_iMaxProcessedUsersNumber(iMaxProcessedUsersNumber),
	m_iMessageCount(0),
	m_pCustomChatMangerHandler(nullptr),
	m_iClientConnectionID(0)
{
	TRACE_FUNC("Constructor called\n");

	this->m_iUsernameLength = strlen(szUsername);
	memcpy(this->m_szUsername, szUsername, this->m_iUsernameLength + NULL_TERMINATE_BYTE_SIZE);

	this->m_pNetwork = new CNetworkTCP(this->m_bIsHost, pszIP, iPort, m_iMaxProcessedUsersNumber);
	this->m_pChatData = new CChatData();
}

CNetworkChatManager::~CNetworkChatManager()
{
	TRACE_FUNC("Destructor called\n");

	auto pCustomChatMangerHandler = this->m_pCustomChatMangerHandler;

	if (pCustomChatMangerHandler != nullptr)
		pCustomChatMangerHandler->OnRelease();

	delete this->m_pNetwork;

	delete this->m_pChatData;
}

bool CNetworkChatManager::Initialize()
{
	if (this->m_bIsInitialized)
		return true;

	this->m_bIsInitialized = GetNetwork()->Startup();

	if (!this->m_bIsInitialized)
	{
		this->SetError(GetNetwork()->What());
		return false;
	}

	GetNetwork()->AddClientsConnectionNotificationCallback(OnConnectionNotification, this);
	GetNetwork()->AddClientsDisconnectionNotificationCallback(OnDisconnectionNotification, this);

	if (IsHost())
		this->m_bIsAdmin = true;
	else
		SendConnectedMessage();

	return this->m_bIsInitialized;
}

void CNetworkChatManager::Shutdown()
{
	this->m_bNeedExit = true;
}

void CNetworkChatManager::ForceExit()
{
	Shutdown();
	this->m_pNetwork->ForceExit();
}

void CNetworkChatManager::ReceivePacketsRoutine()
{
	if (GetNetwork()->ServerWasDowned())
		Shutdown();

	net_packet_t Packet;

	while (GetNetwork()->GetReceivedData(&Packet))
	{
		auto ConnectionID = Packet.m_iConnectionID;
		auto pData = (char*)Packet.m_pPacket;
		auto DataSize = Packet.m_iSize;

		TRACE_FUNC("Packet.m_iConnectionID: %d, Packet.m_pPacket: %p, Packet.m_iSize: %d\n", ConnectionID, pData, DataSize);

		chat_packet_data_t packet_data(pData, DataSize);

		auto szMsgType = PacketReadNetString(packet_data);

		auto User = GetUser(ConnectionID);

		if (IsHost())
		{
			if (!User->m_bConnected)
			{
				TRACE_FUNC("Client not connected. ID: %d\n", ConnectionID);
				continue;
			}
		}

		switch (GetMsgType(szMsgType))
		{
		case MSG_SEND_CHAT_TO_HOST:
			ReceiveSendChatToHost(packet_data, User, ConnectionID);
			break;
		case MSG_SEND_CHAT_TO_CLIENT:
			ReceiveSendChatToClient(packet_data, User, ConnectionID);
			break;
		case MSG_ADMIN_REQUEST:
			ReceiveAdminRequest(packet_data, User, ConnectionID);
			break;
		case MSG_ADMIN_STATUS:
			ReceiveAdminStatus(packet_data, User, ConnectionID);
			break;
		case MSG_CONNECTED:
			ReceiveConnected(packet_data, User, ConnectionID);
			break;
		case MSG_CLIENT_CHAT_USER_ID:
			ReceiveClientChatUserID(packet_data, User, ConnectionID);
			break;
		case MSG_DELETE:
			ReceiveDelete(packet_data, User, ConnectionID);
			break;
		case MSG_ONLINE_LIST:
			ReceiveOnlineList(packet_data, User, ConnectionID);
			break;
		default:
		{
			auto pCustomChatMangerHandler = this->m_pCustomChatMangerHandler;

			if (pCustomChatMangerHandler != nullptr)
			{
				packet_data.reset_process_step();
				pCustomChatMangerHandler->ReceivePacketRoutine(packet_data, ConnectionID, User);
			}
			break;
		}
		}
	}
}

void CNetworkChatManager::ReceiveSendChatToHost(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData)
{
	auto iMessageLength = 0;
	auto szMessage = PacketReadNetString(pData, &iReadCount, &iMessageLength);

	if (!IsValidStrMessage(szMessage, iMessageLength))
	{
		TRACE_FUNC("Invalid string. ID: %d, Message length: %d\n", iConnectionID, iMessageLength);
		return;
	}

	TRACE_FUNC("To host msg. ID: %d, Username: '%s', Message: '%s'\n", iConnectionID, User->m_szUsername, szMessage);

	auto ChatMsg = CreateClientChatMessage(User->m_szUsername, szMessage, iConnectionID, this->m_iMessageCount);

	if (!ChatMsg)
		return;

	GetChatData()->SendNewMessage(User->m_szUsername, szMessage, iConnectionID, this->m_iMessageCount);
	SendNetMsg(ChatMsg);
	IncreaseMessagesCounter();
}

void CNetworkChatManager::ReceiveSendChatToHost(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID)
{
	ReceiveSendChatToHost(packet_data.m_iProcessStep, User, iConnectionID, packet_data.m_pPacket);
}

void CNetworkChatManager::ReceiveSendChatToClient(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData)
{
	auto iUsernameLength = 0;
	auto szUsername = PacketReadNetString(pData, &iReadCount, &iUsernameLength);

	if (!IsValidStrMessage(szUsername, iUsernameLength))
	{
		TRACE_FUNC("Invalid string. ID: %d, Username length: %d\n", iConnectionID, iUsernameLength);
		return;
	}

	auto iMessageLength = 0;
	auto szMessage = PacketReadNetString(pData, &iReadCount, &iMessageLength);

	if (!IsValidStrMessage(szMessage, iMessageLength))
	{
		TRACE_FUNC("Invalid string. ID: %d, Message length: %d\n", iConnectionID, iMessageLength);
		return;
	}

	auto iMessageCount = PacketReadInteger(pData, &iReadCount);

	auto IsMessageImportant = PacketReadChar(pData, &iReadCount);

	auto iMessageOwnerID = PacketReadInteger(pData, &iReadCount);

	TRACE_FUNC("To client msg. Username: '%s' Message: '%s' Message count: %d Message important: %d iMessageOwnerID: %d\n", szUsername, szMessage, iMessageCount, IsMessageImportant, iMessageOwnerID);

	GetChatData()->SendNewMessage(szUsername, szMessage, iMessageOwnerID, iMessageCount, IsMessageImportant);
}

void CNetworkChatManager::ReceiveSendChatToClient(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID)
{
	ReceiveSendChatToClient(packet_data.m_iProcessStep, User, iConnectionID, packet_data.m_pPacket);
}

void CNetworkChatManager::ReceiveAdminRequest(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData)
{
	if (!IsHost())
		return;

	auto szLogin = PacketReadNetString(pData, &iReadCount);
	auto szPassword = PacketReadNetString(pData, &iReadCount);

	TRACE_FUNC("Admin access request from ID: %d, Login: '%s', Password: '%s'\n", iConnectionID, szLogin, szPassword);

	auto IsGranted = GrantAdmin(szLogin, szPassword, iConnectionID);

	if (IsGranted)
	{
		TRACE_FUNC("Admin access granted!\n");
	}
	else
	{
		TRACE_FUNC("Admin not granted!\n");
	}

	SendStatusAdmin(iConnectionID, IsGranted);

	if (IsGranted && this->m_pCustomChatMangerHandler)
	{
		this->m_pCustomChatMangerHandler->OnAdminAllowed(iConnectionID, User);
	}
}

void CNetworkChatManager::ReceiveAdminRequest(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID)
{
	ReceiveAdminRequest(packet_data.m_iProcessStep, User, iConnectionID, packet_data.m_pPacket);
}

void CNetworkChatManager::ReceiveAdminStatus(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData)
{
	if (IsHost())
		return;

	int iMessageLength = 0;
	auto szMessage = PacketReadNetString(pData, &iReadCount, &iMessageLength);

	if (!IsValidStrMessage(szMessage, iMessageLength))
	{
		TRACE_FUNC("Invalid string. Message length: %d\n", iMessageLength);
		return;
	}

	if (!strcmp(szMessage, ADMIN_GRANTED))
	{
		TRACE_FUNC("Server allowed admin\n");
		this->m_bIsAdmin = true;
	}
	else
		TRACE_FUNC("Server disallowed admin\n");

	GetChatData()->SendNewMessage((char*)"SYSTEM", szMessage, 0);
}

void CNetworkChatManager::ReceiveAdminStatus(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID)
{
	ReceiveAdminStatus(packet_data.m_iProcessStep, User, iConnectionID, packet_data.m_pPacket);
}

void CNetworkChatManager::ReceiveConnected(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData)
{
	auto iUsernameLength = 0;
	auto szUsername = PacketReadNetString(pData, &iReadCount, &iUsernameLength);

	if (iUsernameLength > 32)
	{
		TRACE_FUNC("Username too long. ID: %d, Username length: %d\n", iConnectionID, iUsernameLength);
		return;
	}

	if (!IsValidStrMessage(szUsername, iUsernameLength))
	{
		TRACE_FUNC("Invalid string. ID: %d, Username length: %d\n", iConnectionID, iUsernameLength);
		return;
	}

	TRACE_FUNC("Chat client connected. ID: %d, Username: %s\n", iConnectionID, szUsername);

	memcpy(User->m_szUsername, szUsername, iUsernameLength + NULL_TERMINATE_BYTE_SIZE);

	if (IsHost())
	{
		auto NetMsg = CreateNetMsg(CLIENT_CHAT_USER_ID, sizeof(netconnectcount));

		PacketWriteInteger(NetMsg, iConnectionID);

		if (NetMsg)
		{
			SendNetMsgIncludeID(NetMsg, iConnectionID);
			ResendLastMessagesToClient(iConnectionID, 1000);
		}

		auto pCustomChatMangerHandler = this->m_pCustomChatMangerHandler;

		if (pCustomChatMangerHandler != nullptr)
			pCustomChatMangerHandler->OnConnectUser(iConnectionID, User);
	}
}

void CNetworkChatManager::ReceiveConnected(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID)
{
	ReceiveConnected(packet_data.m_iProcessStep, User, iConnectionID, packet_data.m_pPacket);
}

void CNetworkChatManager::ReceiveClientChatUserID(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData)
{
	this->m_iClientConnectionID = PacketReadInteger(pData, &iReadCount);
	TRACE_FUNC("Received connection ID from host: %d\n", this->m_iClientConnectionID);
}

void CNetworkChatManager::ReceiveClientChatUserID(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID)
{
	ReceiveClientChatUserID(packet_data.m_iProcessStep, User, iConnectionID, packet_data.m_pPacket);
}

void CNetworkChatManager::ReceiveDelete(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData, int iDataSize)
{
	if (IsHost() && !User->m_bIsAdmin)
	{
		TRACE_FUNC("User ID: %d not admin\n", iConnectionID);
		return;
	}

	auto iArraySize = PacketReadInteger(pData, &iReadCount);

	TRACE_FUNC("Messages for delete array size: %d Numbers: ", iArraySize);

	for (auto i = 0; i < iArraySize; i++)
	{
		auto iMessageID = PacketReadInteger(pData, &iReadCount);
		TRACE("%d ", iMessageID);
		GetChatData()->DeleteMessage(iMessageID);
	}

	TRACE("\n");

	if (IsHost())
		GetNetwork()->SendPacket(pData, iDataSize);
}

void CNetworkChatManager::ReceiveDelete(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID)
{
	ReceiveDelete(packet_data.m_iProcessStep, User, iConnectionID, packet_data.m_pPacket, packet_data.m_iMessageLength);
}

void CNetworkChatManager::ReceiveOnlineList(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData)
{
	if (IsHost())
		return;

	this->m_iUsersConnectedToHost = PacketReadInteger(pData, &iReadCount);
}

void CNetworkChatManager::ReceiveOnlineList(chat_packet_data_t& packet_data, chat_user* User, netconnectcount iConnectionID)
{
	if (IsHost())
		return;

	this->m_iUsersConnectedToHost = PacketReadInteger(packet_data.m_pPacket, &packet_data.m_iProcessStep);
}

void CNetworkChatManager::SendChatMessage(char* szMessage)
{
	if (IsHost())
	{
		auto ChatMsg = CreateClientChatMessage(this->m_szUsername, szMessage, this->m_iClientConnectionID, this->m_iMessageCount);

		if (ChatMsg)
		{
			GetChatData()->SendNewMessage(this->m_szUsername, szMessage, this->m_iClientConnectionID, this->m_iMessageCount);
			SendNetMsg(ChatMsg);
			IncreaseMessagesCounter();
		}
	}
	else
	{
		SendHostChatMessage(szMessage);
	}
}

void CNetworkChatManager::AddCustomChatHandler(ICustomChatHandler* pCustomChatHandler)
{
	this->m_pCustomChatMangerHandler = pCustomChatHandler;
	this->m_pCustomChatMangerHandler->OnInit(this);
}

size_t CNetworkChatManager::GetChatArraySize()
{
	return GetChatData()->GetMessagesArraySize();
}

bool CNetworkChatManager::IsNeedExit()
{
	return this->m_bNeedExit;
}

bool CNetworkChatManager::IsHost()
{
	return GetNetwork()->IsHost() && this->m_bIsHost;
}

void CNetworkChatManager::AddUser(netconnectcount iConnectionID, int IP, int iPort)
{
	chat_user user{};
	user.m_bConnected = true;
	user.m_IP = IP;
	user.m_iPort = iPort;
	user.m_bIsAdmin = false;
	this->m_vUsersList[iConnectionID] = user;
}

void CNetworkChatManager::ResendLastMessagesToClient(netconnectcount iConnectionID, int iNumberToResend)
{
	TRACE_FUNC("Resend last %d messages for ID: %d\n", iNumberToResend, iConnectionID);

	auto iBeginFrom = (std::int64_t)GetChatData()->GetMessagesArraySize() - iNumberToResend;

	if (iBeginFrom < 0)
		iBeginFrom = 0;

	int i = 0;
	for (auto it = GetChatData()->At(iBeginFrom); it != GetChatData()->End() && i < iNumberToResend; it++, i++)
	{
		auto message = *it;

		if (!message)
			continue;

		auto ChatMsg = CreateClientChatMessage(message->m_szUsername, message->m_szMessage, message->m_iMessageOwnerID, message->m_iMessageID, false);

		if (!ChatMsg)
			continue;

		SendNetMsgIncludeID(ChatMsg, iConnectionID);
	}
}

bool CNetworkChatManager::DisconnectUser(netconnectcount iConnectionID)
{
	auto User = GetUser(iConnectionID);

	if (!User->m_bConnected)
		return false;

	auto pCustomChatMangerHandler = this->m_pCustomChatMangerHandler;

	if (pCustomChatMangerHandler != nullptr)
		pCustomChatMangerHandler->OnDisconnectionUser(iConnectionID, User);

	User->m_bConnected = false;
	User->m_bIsAdmin = false;

	return true;
}

bool CNetworkChatManager::KickUser(netconnectcount iConnectionID)
{
	DisconnectUser(iConnectionID);
	this->m_pNetwork->DisconnectUser(iConnectionID);
}

bool CNetworkChatManager::RequestAdmin(char* szLogin, char* szPassword)
{
	if (!szLogin || !szPassword)
		return false;

	int szLoginLength = strlen(szLogin);
	int szPasswordLength = strlen(szPassword);

	if (szLoginLength < 1 || szPasswordLength < 1)
		return false;

	int MsgSize = CalcNetString(szLoginLength) + CalcNetString(szPasswordLength);

	auto NetMsg = CreateNetMsg(ADMIN_REQUEST_MSG, MsgSize);

	PacketWriteNetString(NetMsg, szLogin, szLoginLength);

	PacketWriteNetString(NetMsg, szPassword, szPasswordLength);

	if (NetMsg)
		SendNetMsg(NetMsg);

	return true;
}

void CNetworkChatManager::SendConnectedMessage(netconnectcount iConnectionID)
{
	auto iUsernameLength = strlen(this->m_szUsername);

	auto MsgSize = CalcNetString(iUsernameLength);

	auto NetMsg = CreateNetMsg(CLIENT_CONNECTED_MSG, MsgSize);

	PacketWriteNetString(NetMsg, this->m_szUsername, iUsernameLength);

	if (NetMsg)
		SendNetMsgIncludeID(NetMsg, iConnectionID);
}

bool CNetworkChatManager::DeleteChatMessage(std::vector<int>* MsgsList)
{
	if (!IsHost() && !IsAdmin())
		return false;

	auto MsgSize = sizeof(int) + (MsgsList->size() * sizeof(int));

	auto NetMsg = CreateNetMsg(DELETE_CHAT_MSG, MsgSize);

	PacketWriteInteger(NetMsg, MsgsList->size());

	for (auto ID : *MsgsList)
	{
		if (IsHost())
			GetChatData()->DeleteMessage(ID);

		PacketWriteInteger(NetMsg, ID);
	}

	if (NetMsg)
		SendNetMsg(NetMsg);

	return true;
}

bool CNetworkChatManager::IsAdmin()
{
	return this->m_bIsAdmin;
}

netconnectcount CNetworkChatManager::GetActiveUsers()
{
	if (IsHost())
		return this->m_pNetwork->GetConnectedUsersCount();

	return this->m_iUsersConnectedToHost;
}

void CNetworkChatManager::SendActiveUsersToClients()
{
	if (!IsHost())
		return;

	auto iConnectedUsers = GetActiveUsers();

	if (iConnectedUsers != this->m_iUsersConnectedToHostPrevFrame)
	{
		auto NetMsg = CreateNetMsg(ONLINE_LIST_MSG, sizeof(netconnectcount));

		PacketWriteInteger(NetMsg, GetActiveUsers());

		if (NetMsg)
		{
			SendNetMsg(NetMsg);
		}	
	}

	this->m_iUsersConnectedToHostPrevFrame = iConnectedUsers;
}

CSafeObject<CChatData> CNetworkChatManager::GetChatData()
{
	return CSafeObject<CChatData>(this->m_pChatData, &this->m_mtxChatData);
}

netconnectcount CNetworkChatManager::GetClientConnectionID()
{
	return this->m_iClientConnectionID;
}

char CNetworkChatManager::PacketReadChar(char* pData, int* pReadCount)
{
	auto ret = pData + *pReadCount;
	*pReadCount += sizeof(char);
	return *(char*)ret;
}

int CNetworkChatManager::PacketReadInteger(char* pData, int* pReadCount)
{
	auto ret = pData + *pReadCount;
	*pReadCount += sizeof(int);
	return *(int*)ret;
}

char* CNetworkChatManager::PacketReadData(char* pData, int iReadingSize, int* pReadCount)
{
	auto ret = pData + *pReadCount;
	*pReadCount += iReadingSize;
	return ret;
}

char* CNetworkChatManager::PacketReadString(char* pData, int iStrLength, int* pReadCount)
{
	if (iStrLength <= 0)
		return nullptr;

	auto ret = pData + *pReadCount;
	*pReadCount += iStrLength + NULL_TERMINATE_BYTE_SIZE;
	return ret;
}

char* CNetworkChatManager::PacketReadNetString(char* pData, int* pReadCount, int* pStrLength)
{
	int iStrLength = 0;

	iStrLength = PacketReadInteger(pData, pReadCount);

	if (pStrLength != nullptr)
		*pStrLength = iStrLength;

	auto szStr = (char*)PacketReadString(pData, iStrLength, pReadCount);

	return szStr;
}

char CNetworkChatManager::PacketReadChar(chat_packet_data_t& packet_data)
{
	return PacketReadChar(packet_data.m_pPacket, &packet_data.m_iProcessStep);
}

int CNetworkChatManager::PacketReadInteger(chat_packet_data_t& packet_data)
{
	return PacketReadInteger(packet_data.m_pPacket, &packet_data.m_iProcessStep);
}

char* CNetworkChatManager::PacketReadData(chat_packet_data_t& packet_data, int iReadingSize)
{
	return PacketReadData(packet_data.m_pPacket, iReadingSize, &packet_data.m_iProcessStep);
}

char* CNetworkChatManager::PacketReadString(chat_packet_data_t& packet_data, int iStrLength)
{
	return PacketReadString(packet_data.m_pPacket, iStrLength, &packet_data.m_iProcessStep);
}

char* CNetworkChatManager::PacketReadNetString(chat_packet_data_t& packet_data, int* pStrLength)
{
	return PacketReadNetString(packet_data.m_pPacket, &packet_data.m_iProcessStep, pStrLength);
}

void CNetworkChatManager::PacketWriteChar(char* pData, int* pWriteCount, char cValue)
{
	*(char*)(pData + *pWriteCount) = cValue;
	*pWriteCount += sizeof(char);
}

void CNetworkChatManager::PacketWriteInteger(char* pData, int* pWriteCount, int iValue)
{
	*(int*)(pData + *pWriteCount) = iValue;
	*pWriteCount += sizeof(int);
}

void CNetworkChatManager::PacketWriteData(char* pData, int* pWriteCount, char* szValue, int iValueLength)
{
	memcpy(pData + *pWriteCount, szValue, iValueLength);
	*pWriteCount += iValueLength;
}

void CNetworkChatManager::PacketWriteString(char* pData, int* pWriteCount, char* szValue, int iValueLength)
{
	if (iValueLength <= 0)
		return;

	PacketWriteData(pData, pWriteCount, szValue, iValueLength);

	*(pData + *pWriteCount) = NULL_TERMINATE_BYTE;
	*pWriteCount += NULL_TERMINATE_BYTE_SIZE;
}

void CNetworkChatManager::PacketWriteNetString(char* pData, int* pWriteCount, char* szValue, int iValueLength)
{
	if (!szValue || !*szValue)
		iValueLength = 0;

	PacketWriteInteger(pData, pWriteCount, iValueLength);
	PacketWriteString(pData, pWriteCount, szValue, iValueLength);
}

void CNetworkChatManager::PacketWriteChar(chat_packet_data_t& packet_data, char cValue)
{
	PacketWriteChar(packet_data.m_pPacket, &packet_data.m_iProcessStep, cValue);
}

void CNetworkChatManager::PacketWriteInteger(chat_packet_data_t& packet_data, int iValue)
{
	PacketWriteInteger(packet_data.m_pPacket, &packet_data.m_iProcessStep, iValue);
}

void CNetworkChatManager::PacketWriteString(chat_packet_data_t& packet_data, char* szValue, int iValueLength)
{
	PacketWriteString(packet_data.m_pPacket, &packet_data.m_iProcessStep, szValue, iValueLength);
}

void CNetworkChatManager::PacketWriteData(chat_packet_data_t& packet_data, char* szValue, int iValueLength)
{
	PacketWriteData(packet_data.m_pPacket, &packet_data.m_iProcessStep, szValue, iValueLength);
}

void CNetworkChatManager::PacketWriteNetString(chat_packet_data_t& packet_data, char* szValue, int iValueLength)
{
	PacketWriteNetString(packet_data.m_pPacket, &packet_data.m_iProcessStep, szValue, iValueLength);
}

bool CNetworkChatManager::IsValidStrMessage(char* szString, int iStringLength)
{
	return *(szString) != NULL_TERMINATE_BYTE && *(szString + iStringLength) == NULL_TERMINATE_BYTE;
}

void CNetworkChatManager::IncreaseMessagesCounter()
{
	this->m_iMessageCount++;
}

chat_packet_data_t CNetworkChatManager::CreateNetMsg(const char* szMsgType, int iMessageSize)
{
	if (szMsgType == nullptr)
		return chat_packet_data_t();

	int iMsgTypeSize = strlen(szMsgType);

	int iPacketSize = CalcNetString(iMsgTypeSize) + CalcNetData(iMessageSize);

	auto NetMsg = chat_packet_data_t(iPacketSize);

	PacketWriteNetString(NetMsg, (char*)szMsgType, iMsgTypeSize);

	return NetMsg;
}

chat_packet_data_t CNetworkChatManager::CreateClientChatMessage(char* szUsername, char* szMessage, netconnectcount iMessageOwnerID, int iMessageID, bool bMessageIsImportant)
{
	auto iUsernameLength = strlen(szUsername);
	auto iMessageLength = strlen(szMessage);

	if (!iUsernameLength || !iMessageLength)
		return chat_packet_data_t();

	int MsgSize = CalcNetString(iUsernameLength) + CalcNetString(iMessageLength) + sizeof(int) + sizeof(bool) + sizeof(netconnectcount);

	auto ChatPacket = CreateNetMsg(SEND_CHAT_TO_CLIENT_MSG, MsgSize);

	PacketWriteNetString(ChatPacket, szUsername, iUsernameLength);

	PacketWriteNetString(ChatPacket, szMessage, iMessageLength);

	PacketWriteInteger(ChatPacket, iMessageID);

	PacketWriteChar(ChatPacket, bMessageIsImportant);

	PacketWriteInteger(ChatPacket, iMessageOwnerID);

	return ChatPacket;
}

bool CNetworkChatManager::SendHostChatMessage(char* szMessage)
{
	auto iMessageLength = strlen(szMessage);

	if (!iMessageLength)
		return false;

	int MsgSize = CalcNetString(iMessageLength);

	auto NetMsg = CreateNetMsg(SEND_CHAT_TO_HOST_MSG, MsgSize);

	PacketWriteNetString(NetMsg, szMessage, iMessageLength);

	if (NetMsg)
		SendNetMsg(NetMsg);

	return true;
}

int CNetworkChatManager::CalcNetData(int iValueLength)
{
	return iValueLength <= 0 ? 0 : iValueLength;
}

int CNetworkChatManager::CalcNetString(int iValueLength)
{
	auto ret = CalcNetData(iValueLength);

	if (ret > 0)
		ret += NULL_TERMINATE_BYTE_SIZE;

	return sizeof(int) + ret;
}

int CNetworkChatManager::CalcStrLen(const char* pszStr)
{
	if (!pszStr)
		return 0;

	return strlen(pszStr);
}

MSG_TYPE CNetworkChatManager::GetMsgType(char* szMsg)
{
	MSG_TYPE ret = MSG_TYPE::MSG_NONE;

	if (!strcmp(szMsg, SEND_CHAT_TO_HOST_MSG))
		ret = MSG_SEND_CHAT_TO_HOST;
	else if (!strcmp(szMsg, SEND_CHAT_TO_CLIENT_MSG))
		ret = MSG_SEND_CHAT_TO_CLIENT;
	else if (!strcmp(szMsg, ADMIN_REQUEST_MSG))
		ret = MSG_ADMIN_REQUEST;
	else if (!strcmp(szMsg, ADMIN_STATUS_MSG))
		ret = MSG_ADMIN_STATUS;
	else if (!strcmp(szMsg, CLIENT_CONNECTED_MSG))
		ret = MSG_CONNECTED;
	else if (!strcmp(szMsg, CLIENT_CHAT_USER_ID))
		ret = MSG_CLIENT_CHAT_USER_ID;
	else if (!strcmp(szMsg, DELETE_CHAT_MSG))
		ret = MSG_DELETE;
	else if (!strcmp(szMsg, ONLINE_LIST_MSG))
		ret = MSG_ONLINE_LIST;

	return ret;
}

void CNetworkChatManager::CheckPacketValid(chat_packet_data_t& chat_packet_data)
{
	if (chat_packet_data.m_iMessageLength != chat_packet_data.m_iProcessStep)
	{
		/*std::*/printf("Invalid packet parameters. NetPacket.m_iMessageLength: %d NetPacket.m_iProcessStep: %d\n", chat_packet_data.m_iMessageLength, chat_packet_data.m_iProcessStep);
		std::abort();
	}
}

void CNetworkChatManager::SendNetMsg(chat_packet_data_t& chat_packet_data)
{
	CheckPacketValid(chat_packet_data);
	GetNetwork()->SendPacket(chat_packet_data.m_pPacket, chat_packet_data.m_iMessageLength);
}

void CNetworkChatManager::SendNetMsgIncludeID(chat_packet_data_t& chat_packet_data, netconnectcount iConnectionID)
{
	CheckPacketValid(chat_packet_data);
	GetNetwork()->SendPacketIncludeID(chat_packet_data.m_pPacket, chat_packet_data.m_iMessageLength, iConnectionID);
}

void CNetworkChatManager::SendNetMsgExcludeID(chat_packet_data_t& chat_packet_data, netconnectcount iConnectionID)
{
	CheckPacketValid(chat_packet_data);
	GetNetwork()->SendPacketExcludeID(chat_packet_data.m_pPacket, chat_packet_data.m_iMessageLength, iConnectionID);
}

void CNetworkChatManager::SendNetMsgWithCondition(chat_packet_data_t& chat_packet_data, fSendConditionSort f)
{
	CheckPacketValid(chat_packet_data);

	for (auto& u : this->m_vUsersList)
	{
		if (!f(u.first, &u.second))
			continue;

		GetNetwork()->SendPacketIncludeID(chat_packet_data.m_pPacket, chat_packet_data.m_iMessageLength, u.first);
	}
}

bool CNetworkChatManager::AddAdminAccessLoginPassword(const char szLogin[128], const char szPassword[128])
{
	if (!IsHost())
	{
		this->SetError("You are not host");
		return false;
	}

	admin_access_login_password_s data{};
	memcpy(data.m_szLogin, szLogin, strlen(szLogin) + 1);
	memcpy(data.m_szPassword, szPassword, strlen(szPassword) + 1);
	this->m_vLoginPassData.push_back(data);
	return true;
}

bool CNetworkChatManager::GrantAdmin(char* szLogin, char* szPassword, netconnectcount iConnectionID)
{
	for (auto& data : this->m_vLoginPassData)
	{
		if (!strcmp(szLogin, data.m_szLogin) && !strcmp(szPassword, data.m_szPassword))
		{
			auto User = GetUser(iConnectionID);

			if (!User->m_bConnected)
				return false;

			User->m_bIsAdmin = true;

			return true;
		}
	}

	return false;
}

void CNetworkChatManager::SendStatusAdmin(netconnectcount iConnectionID, bool IsGranted)
{
	auto Status = IsGranted ? (char*)ADMIN_GRANTED : (char*)ADMIN_NOT_GRANTED;

	auto iStatusLength = strlen(Status);

	auto MsgSize = CalcNetString(iStatusLength);

	auto NetMsg = CreateNetMsg(ADMIN_STATUS_MSG, MsgSize);

	PacketWriteNetString(NetMsg, Status, iStatusLength);

	if (NetMsg)
		SendNetMsgIncludeID(NetMsg, iConnectionID);
}

chat_user* CNetworkChatManager::GetUser(netconnectcount iConnectionID)
{
	return &this->m_vUsersList[iConnectionID];
}

CNetworkTCP* CNetworkChatManager::GetNetwork()
{
	return this->m_pNetwork;
}