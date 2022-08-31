#include "../../includes.h"

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

bool CNetworkChatManager::OnConnectionNotification(bool bIsPreConnectionStep, unsigned int iConnectionCount, int iIp, char* szIP, int iPort, NotificationCallbackUserDataPtr pUserData)
{
	auto pNetworkChatManager = (CNetworkChatManager*)pUserData;

	if (bIsPreConnectionStep)
	{
		TRACE_FUNC("Connected. iConnectionCount: %d, %s:%d\n", iConnectionCount, szIP, iPort);
		pNetworkChatManager->AddUser(iConnectionCount, iIp, iPort);
	}
	else
	{
		pNetworkChatManager->SendConnectedMessage(iConnectionCount);
	}

	return true;
}

bool CNetworkChatManager::OnDisconnectionNotification(unsigned int iConnectionCount, NotificationCallbackUserDataPtr pUserData)
{
	auto pNetworkChatManager = (CNetworkChatManager*)pUserData;

	TRACE_FUNC("Disconnected. iConnectionCount: %d\n", iConnectionCount);

	return pNetworkChatManager->DisconnectUser(iConnectionCount);
}

CNetworkChatManager::CNetworkChatManager(bool IsHost, char* szUsername, char* pszIP, int iPort, int iMaxProcessedUsersNumber) :
	m_bIsInitialized(false),
	m_bIsHost(IsHost),
	m_bIsAdmin(false),
	m_bNeedExit(false),
	m_iUsersConnectedToHost(0),
	m_iUsersConnectedToHostPrevFrame(0),
	m_iMaxProcessedUsersNumber(iMaxProcessedUsersNumber),
	m_iMessageCount(0)
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

		auto Msg = PacketReadMsgType(pData, &iReadCount);

		auto iMsgLength = PacketReadInteger(pData, &iReadCount);

		auto User = GetUser(ConnectionID);

		if (IsHost())
		{
			if (!User->m_bConnected)
			{
				TRACE_FUNC("Client not connected. ID: %d\n", ConnectionID);
				continue;
			}
		}

		switch (Msg)
		{
		case MSG_SEND_CHAT_TO_HOST:
			ReceiveSendChatToHost(iReadCount, User, ConnectionID, pData, DataSize);
			break;
		case MSG_SEND_CHAT_TO_CLIENT:
			ReceiveSendChatToClient(iReadCount, User, ConnectionID, pData, DataSize);
			break;
		case MSG_ADMIN_REQUEST:
			ReceiveAdminRequest(iReadCount, User, ConnectionID, pData, DataSize);
			break;
		case MSG_ADMIN_STATUS:
			ReceiveAdminStatus(iReadCount, User, ConnectionID, pData, DataSize);
			break;
		case MSG_CONNECTED:
			ReceiveConnected(iReadCount, User, ConnectionID, pData, DataSize);
			break;
		case MSG_CLIENT_CHAT_USER_ID:
			ReceiveClientChatUserID(iReadCount, User, ConnectionID, pData, DataSize);
			break;
		case MSG_DELETE:
			ReceiveDelete(iReadCount, User, ConnectionID, pData, DataSize);
			break;
		case MSG_ONLINE_LIST:
			ReceiveOnlineList(iReadCount, User, ConnectionID, pData, DataSize);
			break;
		default:
			TRACE_FUNC("Not valid msg\n");
			break;
		}

		delete[] pData;
	}
}

void CNetworkChatManager::ReceiveSendChatToHost(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize)
{
	auto iMessageLength = 0;
	auto szMessage = PacketReadNetString(pData, &iReadCount, &iMessageLength);

	if (!IsValidStrMessage(szMessage, iMessageLength))
	{
		TRACE_FUNC("Invalid string. ID: %d, Message length: %d\n", iConnectionID, iMessageLength);
		return;
	}

	TRACE_FUNC("To host msg. ID: %d, Username: '%s', Message: '%s'\n", iConnectionID, User->m_szUsername, szMessage);

	auto IsSended = SendNewChatMessageToClient(User->m_szUsername, szMessage, iConnectionID);

	if (IsSended)
	{
		GetChatData()->SendNewMessage(User->m_szUsername, szMessage, iConnectionID, this->m_iMessageCount);
	}
}

void CNetworkChatManager::ReceiveSendChatToClient(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize)
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

void CNetworkChatManager::ReceiveAdminRequest(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize)
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

void CNetworkChatManager::ReceiveAdminStatus(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize)
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

void CNetworkChatManager::ReceiveConnected(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize)
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
		char Packet[4]{};

		int iWriteCount = 0;
		PacketWriteInteger(Packet, &iWriteCount, iConnectionID);

		std::vector<unsigned int> vIDList{ iConnectionID };
		SendNetMsg(MSG_TYPE::MSG_CLIENT_CHAT_USER_ID, Packet, iWriteCount, &vIDList);

		ResendLastMessagesToClient(iConnectionID, 250);
	}
}

void CNetworkChatManager::ReceiveClientChatUserID(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize)
{
	this->m_iClientConnectionID = PacketReadInteger(pData, &iReadCount);
	TRACE_FUNC("Received connection ID from host: %d\n", this->m_iClientConnectionID);
}

void CNetworkChatManager::ReceiveDelete(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize)
{
	if (IsHost() && !User->m_bIsAdmin)
	{
		TRACE_FUNC("User %d not admin\n", iConnectionID);
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

	std::vector<unsigned int> vExcludeID{ iConnectionID };
	GetNetwork()->SendPacketExcludeID(pData, iDataSize, &vExcludeID);
}

void CNetworkChatManager::ReceiveOnlineList(int& iReadCount, chat_user* User, unsigned int iConnectionID, char* pData, int iDataSize)
{
	if (IsHost())
		return;

	this->m_iUsersConnectedToHost = PacketReadInteger(pData, &iReadCount);
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

int CNetworkChatManager::CalcNetData(int iValueLength)
{
	return sizeof(int) + iValueLength;
}

int CNetworkChatManager::CalcNetString(int iValueLength)
{
	return CalcNetData(iValueLength) + NULL_TERMINATE_BYTE_SIZE;
}

MSG_TYPE CNetworkChatManager::PacketReadMsgType(char* pData, int* pReadCount)
{
	auto szMsgType = PacketReadNetString(pData, pReadCount);
	return GetMsgType(szMsgType);
}

bool CNetworkChatManager::IsValidStrMessage(char* szString, int iStringLength)
{
	return *(szString) != NULL_TERMINATE_BYTE && *(szString + iStringLength) == NULL_TERMINATE_BYTE;
}

bool CNetworkChatManager::SendChatMessageToHost(char* szMessage)
{
	auto iMessageLength = strlen(szMessage);

	if (!iMessageLength)
		return false;

	int MsgSize = CalcNetString(iMessageLength);

	char* buff = new char[MsgSize]();

	int iWriteCount = 0;

	PacketWriteNetString(buff, &iWriteCount, szMessage, iMessageLength);

	auto ret = SendNetMsg(MSG_TYPE::MSG_SEND_CHAT_TO_HOST, buff, MsgSize);

	delete[] buff;

	return ret;
}

bool CNetworkChatManager::SendChatMessageToClient(char* szUsername, char* szMessage, unsigned int iMessageOwnerID, int iMessageID, bool bMessageIsImportant, std::vector<unsigned int>* IDList)
{
	auto iUsernameLength = strlen(szUsername);
	auto iMessageLength = strlen(szMessage);

	if (!iUsernameLength || !iMessageLength)
		return false;

	int MsgSize = CalcNetString(iUsernameLength) + CalcNetString(iMessageLength) + sizeof(int) + sizeof(bool) + sizeof(unsigned int);

	char* buff = new char[MsgSize]();

	int iWriteCount = 0;

	PacketWriteNetString(buff, &iWriteCount, szUsername, iUsernameLength);

	PacketWriteNetString(buff, &iWriteCount, szMessage, iMessageLength);

	PacketWriteInteger(buff, &iWriteCount, iMessageID);

	PacketWriteChar(buff, &iWriteCount, bMessageIsImportant);

	PacketWriteInteger(buff, &iWriteCount, iMessageOwnerID);

	auto ret = SendNetMsg(MSG_TYPE::MSG_SEND_CHAT_TO_CLIENT, buff, MsgSize, IDList);

	delete[] buff;

	return ret;
}

bool CNetworkChatManager::SendNewChatMessageToClient(char* szUsername, char* szMessage, unsigned int iMessageOwnerID, std::vector<unsigned int>* IDList)
{
	this->m_iMessageCount++;
	return SendChatMessageToClient(szUsername, szMessage, iMessageOwnerID, this->m_iMessageCount);
}

bool CNetworkChatManager::SendChatMessage(char* szMessage)
{
	auto ret = false;

	if (IsHost())
	{
		ret = SendNewChatMessageToClient(this->m_szUsername, szMessage, 0);

		if (ret)
		{
			GetChatData()->SendNewMessage(this->m_szUsername, szMessage, this->m_iClientConnectionID, this->m_iMessageCount);
		}
	}
	else
	{
		ret = SendChatMessageToHost(szMessage);
	}

	return ret;
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

	char* buff = new char[MsgSize]();

	int iWriteCount = 0;

	PacketWriteNetString(buff, &iWriteCount, szLogin, szLoginLength);

	PacketWriteNetString(buff, &iWriteCount, szPassword, szPasswordLength);

	auto ret = SendNetMsg(MSG_TYPE::MSG_ADMIN_REQUEST, (char*)buff, MsgSize);

	delete[] buff;

	return ret;
}

bool CNetworkChatManager::SendStatusAdmin(unsigned int ID, bool IsGranted)
{
	auto Status = IsGranted ? (char*)ADMIN_GRANTED : (char*)ADMIN_NOT_GRANTED;

	auto iStatusLength = strlen(Status);

	auto MsgSize = CalcNetString(iStatusLength);

	char* buff = new char[MsgSize];

	int iWriteCount = 0;

	PacketWriteNetString(buff, &iWriteCount, Status, iStatusLength);

	std::vector<unsigned int> vList{ ID };
	auto ret = SendNetMsg(MSG_TYPE::MSG_ADMIN_STATUS, (char*)buff, MsgSize, &vList);

	delete[] buff;

	return ret;
}

bool CNetworkChatManager::SendConnectedMessage(unsigned int ID)
{
	auto ret = false;

	auto iUsernameLength = strlen(this->m_szUsername);

	auto MsgSize = CalcNetString(iUsernameLength);

	char* buff = new char[MsgSize];

	int iWriteCount = 0;

	PacketWriteNetString(buff, &iWriteCount, this->m_szUsername, iUsernameLength);

	if (IsHost())
	{
		std::vector<unsigned int> vList{ ID };
		ret = SendNetMsg(MSG_TYPE::MSG_CONNECTED, buff, MsgSize, &vList);
	}
	else
	{
		ret = SendNetMsg(MSG_TYPE::MSG_CONNECTED, buff, MsgSize);
	}

	delete[] buff;

	return ret;
}

bool CNetworkChatManager::DeleteChatMessage(std::vector<int>* MsgsList)
{
	if (!IsHost() && !IsAdmin())
		return false;

	auto MsgSize = sizeof(int) + (MsgsList->size() * sizeof(int));

	char* buff = new char[MsgSize];

	int iWriteCount = 0;

	PacketWriteInteger(buff, &iWriteCount, MsgsList->size());

	for (auto& ID : *MsgsList)
		PacketWriteInteger(buff, &iWriteCount, ID);

	auto ret = SendNetMsg(MSG_TYPE::MSG_DELETE, buff, MsgSize);

	delete[] buff;

	if (ret)
	{
		for (auto& i : *MsgsList)
			GetChatData()->DeleteMessage(i);
	}

	return ret;
}

bool CNetworkChatManager::SendNetMsg(MSG_TYPE MsgType, char* szMessage, int iMessageSize, std::vector<unsigned int>* IDList)
{
	if (!this->m_bIsInitialized)
		return false;

	if (szMessage == nullptr || iMessageSize < 1)
		return false;

	char* szMsgType = (char*)GetStrMsgType(MsgType);

	if (!szMsgType)
		return false;

	int iMsgTypeSize = strlen(szMsgType);

	int iPacketSize = CalcNetString(iMsgTypeSize) + CalcNetData(iMessageSize);

	auto pPacket = new char[iPacketSize];

	int iWriteStep = 0;

	//Msg type
	PacketWriteNetString(pPacket, &iWriteStep, szMsgType, iMsgTypeSize);

	//Msg
	PacketWriteInteger(pPacket, &iWriteStep, iMessageSize);
	PacketWriteData(pPacket, &iWriteStep, szMessage, iMessageSize);

	TRACE_FUNC("iPacketSize: %d, iWriteStep: %d\n", iPacketSize, iWriteStep);

	auto bSendStatus = false;

	bSendStatus = !IDList ?
		GetNetwork()->SendPacketAll(pPacket, iPacketSize) :
		GetNetwork()->SendPacketIncludeID(pPacket, iPacketSize, IDList);

	delete[] pPacket;

	return bSendStatus;
}

bool CNetworkChatManager::SendLocalMsg(char* szAuthor, char* szMessage, int iMessageID)
{
	return GetChatData()->SendNewMessage(szAuthor, szMessage, this->m_iClientConnectionID, iMessageID);
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
	return this->m_bIsHost && GetNetwork()->IsHost();
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

const char* CNetworkChatManager::GetStrMsgType(MSG_TYPE MsgType)
{
	switch (MsgType)
	{
	case MSG_SEND_CHAT_TO_HOST:
		return SEND_CHAT_TO_HOST_MSG;
	case MSG_SEND_CHAT_TO_CLIENT:
		return SEND_CHAT_TO_CLIENT_MSG;
	case MSG_ADMIN_REQUEST:
		return ADMIN_REQUEST_MSG;
	case MSG_ADMIN_STATUS:
		return ADMIN_STATUS_MSG;
	case MSG_CONNECTED:
		return CLIENT_CONNECTED_MSG;
	case MSG_CLIENT_CHAT_USER_ID:
		return CLIENT_CHAT_USER_ID;
	case MSG_DELETE:
		return DELETE_CHAT_MSG;
	case MSG_ONLINE_LIST:
		return ONLINE_LIST_MSG;
	}

	return nullptr;
}

bool CNetworkChatManager::GrantAdmin(char* szLogin, char* szPassword, unsigned int iConnectionID)
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

void CNetworkChatManager::AddUser(unsigned int ID, int IP, int iPort)
{
	chat_user user{};
	user.m_bConnected = true;
	user.m_IP = IP;
	user.m_iPort = iPort;
	user.m_bIsAdmin = false;
	this->m_vUsersList[ID] = user;
}

bool CNetworkChatManager::DisconnectUser(unsigned int ID)
{
	auto User = GetUser(ID);

	if (!User->m_bConnected)
		return false;

	User->m_bConnected = false;
	User->m_bIsAdmin = false;

	return true;
}

void CNetworkChatManager::ResendLastMessagesToClient(unsigned int ID, int iNumberToResend)
{
	TRACE_FUNC("Resend last %d messages to %d client\n", iNumberToResend, ID);

	std::vector<unsigned int> IDList{ ID };

	auto iBeginFrom = (std::int64_t)GetChatData()->GetMessagesArraySize() - iNumberToResend;

	if (iBeginFrom < 0)
		iBeginFrom = 0;

	int i = 0;
	for (auto it = GetChatData()->At(iBeginFrom); it != GetChatData()->End() && i < iNumberToResend; it++, i++)
	{
		auto message = *it;

		if (!message)
			continue;

		SendChatMessageToClient(message->m_szUsername, message->m_szMessage, message->m_iMessageOwnerID, message->m_iMessageID, false, &IDList);
	}
}

bool CNetworkChatManager::IsAdmin()
{
	return this->m_bIsAdmin;
}

unsigned int CNetworkChatManager::GetActiveUsers()
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
		char Packet[4]{};

		int iWriteCount = 0;
		PacketWriteInteger(Packet, &iWriteCount, GetActiveUsers());

		SendNetMsg(MSG_TYPE::MSG_ONLINE_LIST, Packet, iWriteCount);
	}

	this->m_iUsersConnectedToHostPrevFrame = iConnectedUsers;
}

CNetworkTCP* CNetworkChatManager::GetNetwork()
{
	return this->m_pNetwork;
}

CChatData* CNetworkChatManager::GetChatData()
{
	this->m_mtxChatData.lock();
	auto ret = this->m_pChatData;
	this->m_mtxChatData.unlock();
	return ret;
}

chat_user* CNetworkChatManager::GetUser(unsigned int ID)
{
	return &this->m_vUsersList[ID];
}

unsigned int CNetworkChatManager::GetClientConnectionID()
{
	return this->m_iClientConnectionID;
}