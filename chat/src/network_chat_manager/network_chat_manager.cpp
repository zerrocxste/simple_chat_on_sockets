#include "../includes.h"

constexpr auto SEND_CHAT_TO_HOST_MSG = "SEND_CHAT_TO_HOST_MSG";
constexpr auto SEND_CHAT_TO_CLIENT_MSG = "SEND_CHAT_TO_CLIENT_MSG";
constexpr auto ADMIN_REQUEST_MSG = "ADMIN_REQUEST_MSG";
constexpr auto ADMIN_STATUS_MSG = "ADMIN_STATUS_MSG";
constexpr auto CLIENT_CONNECTED_MSG = "CLIENT_CONNECTED_MSG";
constexpr auto DELETE_CHAT_MSG = "DELETE_CHAT_MSG";

constexpr auto ADMIN_GRANTED = "Admin access granted!";
constexpr auto ADMIN_NOT_GRANTED = "Login or password is incorrect";

bool OnConnectionNotification(bool bIsPreConnectionStep, int iConnectionCount, int iIp, char* szIP, int iPort)
{
	if (bIsPreConnectionStep)
	{
		TRACE_FUNC("iConnectionCount: %d, %s:%d\n", iConnectionCount, szIP, iPort);
		g_pNetworkChatManager->AddUser(iConnectionCount, iIp, iPort);
	}
	else
	{
		g_pNetworkChatManager->SendConnectedMessage(iConnectionCount);
		g_pNetworkChatManager->ResendLastMessagesToClient(iConnectionCount, 250);
	}

	return true;
}

bool OnDisconnectionNotification(int iConnectionCount)
{
	TRACE_FUNC("iConnectionCount: %d\n", iConnectionCount);
	return g_pNetworkChatManager->DisconnectUser(iConnectionCount);
}

CNetworkChatManager::CNetworkChatManager(bool IsHost, char* szUsername, char* pszIP, int iPort, int iMaxProcessedUsersNumber) :
	m_bIsInitialized(false),
	m_bIsAdmin(false),
	m_bIsHost(IsHost),
	m_bNeedExit(false),
	m_iMaxProcessedUsersNumber(iMaxProcessedUsersNumber),
	m_iMessageCount(0)
{
	TRACE_FUNC("Constructor called\n");

	this->m_iUsernameLength = strlen(szUsername);
	memcpy(this->m_szUsername, szUsername, this->m_iUsernameLength + NULL_TERMINATE_BYTE_SIZE);

	this->m_pNetwork = new CNetwork(this->m_bIsHost, pszIP, iPort, m_iMaxProcessedUsersNumber);
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
		MessageBox(NULL, "m_pNetwork->Startup failed", "", MB_OK | MB_ICONERROR);
		return false;
	}

	GetNetwork()->AddClientsConnectionNotificationCallback(OnConnectionNotification);
	GetNetwork()->AddClientsDisconnectionNotificationCallback(OnDisconnectionNotification);

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

	net_packet Packet;

	auto HasData = GetNetwork()->GetReceivedData(&Packet);

	if (!HasData)
		return;

	auto ConnectionID = Packet.m_iConnectionID;
	auto pData = (char*)Packet.m_pPacket;
	auto DataSize = Packet.m_iSize;

	if (!DataSize)
		return;

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
			return;
		}
	}

	if (Msg == MSG_SEND_CHAT_TO_HOST)
	{
		auto iMessageLength = 0;
		auto szMessage = PacketReadNetString(pData, &iReadCount, &iMessageLength);

		if (!IsValidStrMessage(szMessage, iMessageLength))
		{
			TRACE_FUNC("Invalid string. ID: %d, Message length: %d\n", ConnectionID, iMessageLength);
			return;
		}

		TRACE_FUNC("To host msg. ID: %d, Username: '%s', Message: '%s'\n", ConnectionID, User->m_szUsername, szMessage);

		auto IsSended = SendNewChatMessageToClient(User->m_szUsername, szMessage);

		if (IsSended)
		{
			GetChatData()->SendNewMessage(User->m_szUsername, szMessage, this->m_iMessageCount);
		}
	}
	else if (Msg == MSG_SEND_CHAT_TO_CLIENT)
	{
		auto iUsernameLength = 0;
		auto szUsername = PacketReadNetString(pData, &iReadCount, &iUsernameLength);

		if (!IsValidStrMessage(szUsername, iUsernameLength))
		{
			TRACE_FUNC("Invalid string. ID: %d, Username length: %d\n", ConnectionID, iUsernameLength);
			return;
		}

		auto iMessageLength = 0;
		auto szMessage = PacketReadNetString(pData, &iReadCount, &iMessageLength);

		if (!IsValidStrMessage(szMessage, iMessageLength))
		{
			TRACE_FUNC("Invalid string. ID: %d, Message length: %d\n", ConnectionID, iMessageLength);
			return;
		}

		auto iMessageCount = PacketReadInteger(pData, &iReadCount);

		TRACE_FUNC("To client msg. Username: '%s' Message: '%s'\n", szUsername, szMessage);

		GetChatData()->SendNewMessage(szUsername, szMessage, iMessageCount);
	}
	else if (Msg == MSG_ADMIN_REQUEST)
	{
		if (!IsHost())
			return;

		auto szLogin = PacketReadNetString(pData, &iReadCount);
		auto szPassword = PacketReadNetString(pData, &iReadCount);

		TRACE_FUNC("Admin access request from ID: %d, Login: '%s', Password: '%s'\n", ConnectionID, szLogin, szPassword);

		auto IsGranted = GrantAdmin(szLogin, szPassword, ConnectionID);

		if (IsGranted)
		{
			TRACE_FUNC("Admin access granted!\n");
		}
		else
		{
			TRACE_FUNC("Admin not granted!\n");
		}

		SendStatusAdmin(ConnectionID, IsGranted);
	}
	else if (Msg == MSG_ADMIN_STATUS)
	{
		if (IsHost())
			return;

		auto szMessage = PacketReadNetString(pData, &iReadCount);

		bool IsGranted = false;

		if (!strcmp(szMessage, ADMIN_GRANTED))
		{
			TRACE_FUNC("Server allowed admin\n");
			this->m_bIsAdmin = true;
		}
		else
			TRACE_FUNC("Server disallowed admin\n");

		GetChatData()->SendNewMessage((char*)"SYSTEM", szMessage);
	}
	else if (Msg == MSG_CONNECTED)
	{
		auto iUsernameLength = 0;
		auto szUsername = PacketReadNetString(pData, &iReadCount, &iUsernameLength);

		if (iUsernameLength > 32)
		{
			TRACE_FUNC("Username too long. ID: %d, Username length: %d\n", ConnectionID, iUsernameLength);
			return;
		}

		if (!IsValidStrMessage(szUsername, iUsernameLength))
		{
			TRACE_FUNC("Invalid string. ID: %d, Username length: %d\n", ConnectionID, iUsernameLength);
			return;
		}

		TRACE_FUNC("Chat client connected. ID: %d, Username: %s\n", ConnectionID, szUsername);

		memcpy(User->m_szUsername, szUsername, iUsernameLength + NULL_TERMINATE_BYTE_SIZE);
	}
	else if (Msg == MSG_DELETE)
	{
		if (IsHost() && !User->m_bIsAdmin)
		{
			TRACE_FUNC("User %d not admin\n", ConnectionID);
			return;
		}

		auto iArraySize = PacketReadInteger(pData, &iReadCount);

		TRACE_FUNC("Messages for delete array size: %d Numbers: ", iArraySize);

		for (auto i = 0; i < iArraySize; i++)
		{
			auto iMessageID = PacketReadInteger(pData, &iReadCount);
			TRACE("%d", iMessageID);
			GetChatData()->DeleteMessage(iMessageID);
		}

		TRACE("\n");

		std::vector<int> vExcludeID{ ConnectionID };
		GetNetwork()->SendPacketExcludeID(Packet.m_pPacket, DataSize, &vExcludeID);
	}
	else
	{
		TRACE_FUNC("Not valid msg\n");
	}
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

bool CNetworkChatManager::SendChatMessageToClient(char* szUsername, char* szMessage, int iMessageID, std::vector<int>* IDList)
{
	auto iUsernameLength = strlen(szUsername);
	auto iMessageLength = strlen(szMessage);

	if (!iUsernameLength || !iMessageLength)
		return false;

	int MsgSize = CalcNetString(iUsernameLength) + CalcNetString(iMessageLength) + sizeof(int);

	char* buff = new char[MsgSize]();

	int iWriteCount = 0;

	PacketWriteNetString(buff, &iWriteCount, szUsername, iUsernameLength);

	PacketWriteNetString(buff, &iWriteCount, szMessage, iMessageLength);

	PacketWriteInteger(buff, &iWriteCount, iMessageID);

	auto ret = SendNetMsg(MSG_TYPE::MSG_SEND_CHAT_TO_CLIENT, buff, MsgSize, IDList);

	delete[] buff;

	return ret;
}

bool CNetworkChatManager::SendNewChatMessageToClient(char* szUsername, char* szMessage, std::vector<int>* IDList)
{
	this->m_iMessageCount++;
	return SendChatMessageToClient(szUsername, szMessage, this->m_iMessageCount);
}

bool CNetworkChatManager::SendChatMessage(char* szMessage)
{
	auto ret = false;

	if (IsHost())
	{
		ret = SendNewChatMessageToClient(this->m_szUsername, szMessage);

		if (ret)
		{
			GetChatData()->SendNewMessage(this->m_szUsername, szMessage, this->m_iMessageCount);
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

bool CNetworkChatManager::SendStatusAdmin(int ID, bool IsGranted)
{
	auto Status = IsGranted ? (char*)ADMIN_GRANTED : (char*)ADMIN_NOT_GRANTED;

	auto iStatusLength = strlen(Status);

	auto MsgSize = CalcNetString(iStatusLength);

	char* buff = new char[MsgSize];

	int iWriteCount = 0;

	PacketWriteNetString(buff, &iWriteCount, Status, iStatusLength);

	std::vector<int> vList{ ID };
	auto ret = SendNetMsg(MSG_TYPE::MSG_ADMIN_STATUS, (char*)buff, MsgSize, &vList);

	delete[] buff;

	return ret;
}

bool CNetworkChatManager::SendConnectedMessage(int ID)
{
	auto ret = false;

	auto iUsernameLength = strlen(this->m_szUsername);

	auto MsgSize = CalcNetString(iUsernameLength);

	char* buff = new char[MsgSize];

	int iWriteCount = 0;

	PacketWriteNetString(buff, &iWriteCount, this->m_szUsername, iUsernameLength);

	if (IsHost())
	{
		std::vector<int> vList{ ID };
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

bool CNetworkChatManager::SendNetMsg(MSG_TYPE MsgType, char* szMessage, int iMessageSize, std::vector<int>* IDList)
{
	if (!this->m_bIsInitialized)
		return false;

	if (szMessage == nullptr || iMessageSize < 1)
		return false;

	char* szMsgType = nullptr;

	switch (MsgType)
	{
	case MSG_SEND_CHAT_TO_HOST:
		szMsgType = (char*)SEND_CHAT_TO_HOST_MSG;
		break;
	case MSG_SEND_CHAT_TO_CLIENT:
		szMsgType = (char*)SEND_CHAT_TO_CLIENT_MSG;
		break;
	case MSG_ADMIN_REQUEST:
		szMsgType = (char*)ADMIN_REQUEST_MSG;
		break;
	case MSG_ADMIN_STATUS:
		szMsgType = (char*)ADMIN_STATUS_MSG;
		break;
	case MSG_CONNECTED:
		szMsgType = (char*)CLIENT_CONNECTED_MSG;
		break;
	case MSG_DELETE:
		szMsgType = (char*)DELETE_CHAT_MSG;
		break;
	default:
		return false;
	}

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
	return GetChatData()->SendNewMessage(szAuthor, szMessage, iMessageID);
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
	else if (!strcmp(szMsg, DELETE_CHAT_MSG))
		ret = MSG_DELETE;

	return ret;
}

bool CNetworkChatManager::GrantAdmin(char* szLogin, char* szPassword, int iConnectionID)
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

void CNetworkChatManager::AddUser(int ID, int IP, int iPort)
{
	chat_user user{};
	user.m_bConnected = true;
	user.m_IP = IP;
	user.m_iPort = iPort;
	user.m_bIsAdmin = false;
	this->m_vUsersList[ID] = user;
}

bool CNetworkChatManager::DisconnectUser(int ID)
{
	auto User = GetUser(ID);

	if (!User->m_bConnected)
		return false;

	User->m_bConnected = false;
	User->m_bIsAdmin = false;

	return true;
}

void CNetworkChatManager::ResendLastMessagesToClient(int ID, int iNumberToResend)
{
	TRACE_FUNC("Resend last %d messages to %d client\n", iNumberToResend, ID);

	std::vector<int> IDList{ ID };

	auto iBeginFrom = (std::int64_t)GetChatData()->GetMessagesArraySize() - iNumberToResend;

	if (iBeginFrom < 0)
		iBeginFrom = 0;

	int i = 0;
	for (auto it = GetChatData()->At(iBeginFrom); it != GetChatData()->End() && i < iNumberToResend; it++, i++)
	{
		auto message = *it;

		if (!message)
			continue;

		SendChatMessageToClient(message->m_szUsername, message->m_szMessage, message->m_iMessageID, &IDList);
	}
}

bool CNetworkChatManager::IsAdmin()
{
	return this->m_bIsAdmin;
}

CNetwork* CNetworkChatManager::GetNetwork()
{
	return this->m_pNetwork;
}

CChatData* CNetworkChatManager::GetChatData()
{
	return this->m_pChatData;
}

chat_user* CNetworkChatManager::GetUser(int ID)
{
	return &this->m_vUsersList[ID];
}