#include "../../includes.h"

chat_packet_data_t::chat_packet_data_t() :
	m_pPacket(nullptr),
	m_iMessageLength(0)
{

}

chat_packet_data_t::chat_packet_data_t(const chat_packet_data_t& PacketData)
{
	*this = PacketData;
}

chat_packet_data_t::chat_packet_data_t(int iMessageLength)
{
	this->m_pPacket = new char[iMessageLength];
	this->m_iMessageLength = iMessageLength;
}

chat_packet_data_t::chat_packet_data_t(char* pPacket, int iMessageLength)
{
	this->m_pPacket = pPacket;
	this->m_iMessageLength = iMessageLength;
}

chat_packet_data_t::~chat_packet_data_t()
{
	if (*this)
		delete[] this->m_pPacket;
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
	this->m_iMessageLength = PacketData.m_iMessageLength;
	PacketData.m_pPacket = nullptr;
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

	if (bIsPreConnectionStep)
	{
		TRACE_FUNC("Connected. ID: %d, %s:%d\n", iConnectionID, szIP, iPort);
		pNetworkChatManager->AddUser(iConnectionID, iIp, iPort);
	}
	else
	{
		pNetworkChatManager->SendConnectedMessage(iConnectionID);
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
	m_pCustomChatMangerHandler(nullptr)
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
		MessageBox(0, "m_pNetwork->Startup failed", "", MB_OK | MB_ICONERROR);
		return false;
	}

	GetNetwork()->AddClientsConnectionNotificationCallback(OnConnectionNotification, this);
	GetNetwork()->AddClientsDisconnectionNotificationCallback(OnDisconnectionNotification, this);

	if (IsHost())
	{
		this->m_bIsAdmin = true;
	}
	else
	{
		SendConnectedMessage();
	}

	return this->m_bIsInitialized;
}

void CNetworkChatManager::Shutdown()
{
	this->m_bNeedExit = true;
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

		int iReadCount = 0;

		auto szMsgType = PacketReadNetString(pData, &iReadCount);

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
			ReceiveSendChatToHost(iReadCount, User, ConnectionID, pData);
			break;
		case MSG_SEND_CHAT_TO_CLIENT:
			ReceiveSendChatToClient(iReadCount, User, ConnectionID, pData);
			break;
		case MSG_ADMIN_REQUEST:
			ReceiveAdminRequest(iReadCount, User, ConnectionID, pData);
			break;
		case MSG_ADMIN_STATUS:
			ReceiveAdminStatus(iReadCount, User, ConnectionID, pData);
			break;
		case MSG_CONNECTED:
			ReceiveConnected(iReadCount, User, ConnectionID, pData);
			break;
		case MSG_CLIENT_CHAT_USER_ID:
			ReceiveClientChatUserID(iReadCount, User, ConnectionID, pData);
			break;
		case MSG_DELETE:
			ReceiveDelete(iReadCount, User, ConnectionID, pData, DataSize);
			break;
		case MSG_ONLINE_LIST:
			ReceiveOnlineList(iReadCount, User, ConnectionID, pData);
			break;
		default:
			if (this->m_pCustomChatMangerHandler != nullptr)
				this->m_pCustomChatMangerHandler->ReceivePacketRoutine(this, szMsgType, iReadCount, ConnectionID, User);
		}

		delete[] pData;
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
		int iWriteCount = 0;

		auto NetMsg = CreateNetMsg(CLIENT_CHAT_USER_ID, sizeof(netconnectcount), &iWriteCount);

		PacketWriteInteger(NetMsg.m_pPacket, &iWriteCount, iConnectionID);

		if (NetMsg)
		{
			SendNetMsgToID(NetMsg, iConnectionID);
			ResendLastMessagesToClient(iConnectionID, 250);
		}
	}
}

void CNetworkChatManager::ReceiveClientChatUserID(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData)
{
	this->m_iClientConnectionID = PacketReadInteger(pData, &iReadCount);
	TRACE_FUNC("Received connection ID from host: %d\n", this->m_iClientConnectionID);
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

void CNetworkChatManager::ReceiveOnlineList(int& iReadCount, chat_user* User, netconnectcount iConnectionID, char* pData)
{
	if (IsHost())
		return;

	this->m_iUsersConnectedToHost = PacketReadInteger(pData, &iReadCount);
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

		SendNetMsgToID(ChatMsg, iConnectionID);
	}
}

bool CNetworkChatManager::DisconnectUser(netconnectcount iConnectionID)
{
	auto User = GetUser(iConnectionID);

	if (!User->m_bConnected)
		return false;

	User->m_bConnected = false;
	User->m_bIsAdmin = false;

	return true;
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

	int iWriteCount = 0;

	auto NetMsg = CreateNetMsg(ADMIN_REQUEST_MSG, MsgSize, &iWriteCount);

	PacketWriteNetString(NetMsg.m_pPacket, &iWriteCount, szLogin, szLoginLength);

	PacketWriteNetString(NetMsg.m_pPacket, &iWriteCount, szPassword, szPasswordLength);

	if (NetMsg)
		SendNetMsg(NetMsg);

	return true;
}

void CNetworkChatManager::SendConnectedMessage(netconnectcount iConnectionID)
{
	auto iUsernameLength = strlen(this->m_szUsername);

	auto MsgSize = CalcNetString(iUsernameLength);

	int iWriteCount = 0;

	auto NetMsg = CreateNetMsg(CLIENT_CONNECTED_MSG, MsgSize, &iWriteCount);

	PacketWriteNetString(NetMsg.m_pPacket, &iWriteCount, this->m_szUsername, iUsernameLength);

	if (NetMsg)
		SendNetMsgToID(NetMsg, iConnectionID);
}

bool CNetworkChatManager::DeleteChatMessage(std::vector<int>* MsgsList)
{
	if (!IsHost() && !IsAdmin())
		return false;

	auto MsgSize = sizeof(int) + (MsgsList->size() * sizeof(int));

	int iWriteCount = 0;

	auto NetMsg = CreateNetMsg(DELETE_CHAT_MSG, MsgSize, &iWriteCount);

	PacketWriteInteger(NetMsg.m_pPacket, &iWriteCount, MsgsList->size());

	for (auto ID : *MsgsList)
	{
		if (IsHost())
			GetChatData()->DeleteMessage(ID);

		PacketWriteInteger(NetMsg.m_pPacket, &iWriteCount, ID);
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
		int iWriteCount = 0;

		auto NetMsg = CreateNetMsg(ONLINE_LIST_MSG, sizeof(netconnectcount), &iWriteCount);

		PacketWriteInteger(NetMsg.m_pPacket, &iWriteCount, GetActiveUsers());

		if (NetMsg)
			SendNetMsg(NetMsg);
	}

	this->m_iUsersConnectedToHostPrevFrame = iConnectedUsers;
}

CChatData* CNetworkChatManager::GetChatData()
{
	this->m_mtxChatData.lock();
	auto ret = this->m_pChatData;
	this->m_mtxChatData.unlock();
	return ret;
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

char* CNetworkChatManager::PacketReadString(char* pData, int iStrLength, int* pReadCount)
{
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
	PacketWriteData(pData, pWriteCount, szValue, iValueLength);

	*(pData + *pWriteCount) = NULL_TERMINATE_BYTE;
	*pWriteCount += NULL_TERMINATE_BYTE_SIZE;
}

void CNetworkChatManager::PacketWriteNetString(char* pData, int* pWriteCount, char* szValue, int iValueLength)
{
	PacketWriteInteger(pData, pWriteCount, iValueLength);
	PacketWriteString(pData, pWriteCount, szValue, iValueLength);
}

bool CNetworkChatManager::IsValidStrMessage(char* szString, int iStringLength)
{
	return *(szString) != NULL_TERMINATE_BYTE && *(szString + iStringLength) == NULL_TERMINATE_BYTE;
}

void CNetworkChatManager::IncreaseMessagesCounter()
{
	this->m_iMessageCount++;
}

chat_packet_data_t CNetworkChatManager::CreateNetMsg(const char* szMsgType, int iMessageSize, int* iWriteCount)
{
	if (szMsgType == nullptr || iMessageSize < 1)
		return chat_packet_data_t();

	int iMsgTypeSize = strlen(szMsgType);

	int iPacketSize = CalcNetString(iMsgTypeSize) + CalcNetData(iMessageSize);

	auto ChatPacket = chat_packet_data_t(iPacketSize);

	PacketWriteNetString(ChatPacket.m_pPacket, iWriteCount, (char*)szMsgType, iMsgTypeSize);

	return ChatPacket;
}

chat_packet_data_t CNetworkChatManager::CreateClientChatMessage(char* szUsername, char* szMessage, netconnectcount iMessageOwnerID, int iMessageID, bool bMessageIsImportant)
{
	auto iUsernameLength = strlen(szUsername);
	auto iMessageLength = strlen(szMessage);

	if (!iUsernameLength || !iMessageLength)
		return chat_packet_data_t();

	int MsgSize = CalcNetString(iUsernameLength) + CalcNetString(iMessageLength) + sizeof(int) + sizeof(bool) + sizeof(netconnectcount);

	int iWriteCount = 0;

	auto ChatPacket = CreateNetMsg(SEND_CHAT_TO_CLIENT_MSG, MsgSize, &iWriteCount);

	PacketWriteNetString(ChatPacket.m_pPacket, &iWriteCount, szUsername, iUsernameLength);

	PacketWriteNetString(ChatPacket.m_pPacket, &iWriteCount, szMessage, iMessageLength);

	PacketWriteInteger(ChatPacket.m_pPacket, &iWriteCount, iMessageID);

	PacketWriteChar(ChatPacket.m_pPacket, &iWriteCount, bMessageIsImportant);

	PacketWriteInteger(ChatPacket.m_pPacket, &iWriteCount, iMessageOwnerID);

	return ChatPacket;
}

bool CNetworkChatManager::SendHostChatMessage(char* szMessage)
{
	auto iMessageLength = strlen(szMessage);

	if (!iMessageLength)
		return false;

	int MsgSize = CalcNetString(iMessageLength);

	int iWriteCount = 0;

	auto NetMsg = CreateNetMsg(SEND_CHAT_TO_HOST_MSG, MsgSize, &iWriteCount);

	PacketWriteNetString(NetMsg.m_pPacket, &iWriteCount, szMessage, iMessageLength);

	if (NetMsg)
		SendNetMsg(NetMsg);

	return true;
}

int CNetworkChatManager::CalcNetData(int iValueLength)
{
	return sizeof(int) + iValueLength;
}

int CNetworkChatManager::CalcNetString(int iValueLength)
{
	return CalcNetData(iValueLength) + NULL_TERMINATE_BYTE_SIZE;
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

void CNetworkChatManager::SendNetMsg(chat_packet_data_t& chat_packet_data)
{
	GetNetwork()->SendPacket(chat_packet_data.m_pPacket, chat_packet_data.m_iMessageLength);
}

void CNetworkChatManager::SendNetMsgToID(chat_packet_data_t& chat_packet_data, netconnectcount iConnectionID)
{
	GetNetwork()->SendPacket(chat_packet_data.m_pPacket, chat_packet_data.m_iMessageLength, iConnectionID);
}

bool CNetworkChatManager::GrantAdmin(char* szLogin, char* szPassword, netconnectcount iConnectionID)
{
	const char* AdminLoginPasswordList[][2] = {
		{ "TestAdmin1", "TestAdmin1Password" },
		{ "TestAdmin2", "TestAdmin2Password" },
		{ "TestAdmin3", "TestAdmin3Password" }
	};

	for (auto i = 0; i < 3; i++)
	{
		if (!strcmp(szLogin, AdminLoginPasswordList[i][0]) && !strcmp(szPassword, AdminLoginPasswordList[i][1]))
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

	int iWriteCount = 0;

	auto NetMsg = CreateNetMsg(ADMIN_STATUS_MSG, MsgSize, &iWriteCount);

	PacketWriteNetString(NetMsg.m_pPacket, &iWriteCount, Status, iStatusLength);

	if (NetMsg)
		SendNetMsgToID(NetMsg, iConnectionID);
}

chat_user* CNetworkChatManager::GetUser(netconnectcount iConnectionID)
{
	return &this->m_vUsersList[iConnectionID];
}

CNetworkTCP* CNetworkChatManager::GetNetwork()
{
	return this->m_pNetwork;
}