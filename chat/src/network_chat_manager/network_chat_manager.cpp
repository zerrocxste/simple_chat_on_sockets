#include "../includes.h"

constexpr auto CHAT_MESSAGE_MSG = "CHAT_MESSAGE_MSG";
constexpr auto ADMIN_REQUEST_MSG = "ADMIN_REQUEST_MSG";
constexpr auto ADMIN_STATUS_MSG = "ADMIN_STATUS_MSG";
constexpr auto DELETE_CHAT_MSG = "DELETE_CHAT_MSG";

constexpr auto ADMIN_GRANTED = "Admin access granted!";
constexpr auto ADMIN_NOT_GRANTED = "Login or password is incorrect";

bool NewConnectionNotification(bool bIsPreConnectionStep, int iConnectionCount, int iIp, char* szIP, int iPort)
{
	if (bIsPreConnectionStep)
	{
		printf("[+] %s() -> iConnectionCount: %d, %s:%d\n", __FUNCTION__, iConnectionCount, szIP, iPort);
		g_pNetworkChatManager->AddUser(iConnectionCount, iIp, iPort);
	}
	else
	{
		g_pNetworkChatManager->ResendLastMessagesToClient(iConnectionCount, 250);
	}	

	return true;
}

CNetworkChatManager::CNetworkChatManager(bool IsHost, char* szUsername, char* pszIP, int iPort, int iMaxProcessedUsersNumber) :
	m_bIsInitialized(false),
	m_bIsAdmin(false),
	m_bIsHost(IsHost),
	m_bNeedExit(false),
	m_iMaxProcessedUsersNumber(iMaxProcessedUsersNumber),
	m_iMessageCount(0)
{
	printf("[+] %s -> Constructor called\n", __FUNCTION__);

	memcpy(this->m_szUsername, szUsername, strlen(szUsername) + NULL_TERMINATE_BYTE_SIZE);
	this->m_pNetwork = new CNetwork(this->m_bIsHost, pszIP, iPort, m_iMaxProcessedUsersNumber);
	this->m_pChatData = new CChatData();

	this->m_pNetwork->AddClientsNotificationCallback(NewConnectionNotification);

	if (IsHost)
		this->m_bIsAdmin = true;
}

CNetworkChatManager::~CNetworkChatManager()
{
	printf("[+] %s -> Destructor called\n", __FUNCTION__);

	delete this->m_pNetwork;
	delete this->m_pChatData;
}

bool CNetworkChatManager::Initialize()
{
	if (this->m_bIsInitialized)
		return true;

	this->m_bIsInitialized = this->m_pNetwork->Startup();

	if (!this->m_bIsInitialized)
	{
		MessageBox(NULL, "m_pNetwork->Startup failed", "", MB_OK | MB_ICONERROR);
		return false;
	}

	return this->m_bIsInitialized;
}

void CNetworkChatManager::Shutdown()
{
	this->m_bNeedExit = true;
}

void CNetworkChatManager::ReceivePacketsRoutine()
{
	if (this->m_pNetwork->ServerWasDowned())
		Shutdown();

	net_packet Packet;

	auto HasData = this->m_pNetwork->GetReceivedData(&Packet);

	if (!HasData)
		return;

	if (!Packet.m_iSize)
		return;

	printf("[+] %s -> Packet.m_pPacket: %p, Packet.m_iSize: %d, Packet.m_iConnectionID: %d\n", __FUNCTION__, Packet.m_pPacket, Packet.m_iSize, Packet.m_iConnectionID);

	auto pData = (char*)Packet.m_pPacket;
	auto pDataSize = Packet.m_iSize;

	int iReadCount = 0;

	auto Msg = PacketReadMsgType(pData, &iReadCount);

	auto szAuthorName = PacketReadNetString(pData, &iReadCount);

	auto iMessageLength = *PacketReadInteger(pData, &iReadCount);

	if (Msg == CHAT_MSG)
	{
		auto szMessage = PacketReadString(pData, iMessageLength, &iReadCount);
		auto pMessageCount = PacketReadInteger(pData, &iReadCount);

		auto IsOk = true;

		if (IsHost())
		{
			this->m_iMessageCount++;
			*pMessageCount = this->m_iMessageCount;
			IsOk = this->m_pNetwork->SendPacketAll(Packet.m_pPacket, Packet.m_iSize);
		}

		if (IsOk)
			this->m_pChatData->SendNewMessage(szAuthorName, szMessage, *pMessageCount);
	}
	else if (Msg == ADMIN_REQUEST)
	{
		if (!IsHost())
			return;

		auto szLogin = PacketReadNetString(pData, &iReadCount);
		auto szPassword = PacketReadNetString(pData, &iReadCount);

		printf("[+] %s() -> Admin access. Login: '%s' Password: '%s'\n", __FUNCTION__, szLogin, szPassword);

		auto IsGranted = GrantAdmin(szLogin, szPassword, Packet.m_iConnectionID);

		if (IsGranted)
		{
			printf("[+] %s() -> '%s', id: %d Admin access granted!\n", __FUNCTION__, szAuthorName, Packet.m_iConnectionID);
		}
		else
		{
			printf("[+] %s() -> '%s', id: %d Admin not granted!\n", __FUNCTION__, szAuthorName, Packet.m_iConnectionID);
		}

		SendStatusAdmin(Packet.m_iConnectionID, IsGranted);
	}
	else if (Msg == ADMIN_STATUS)
	{
		if (IsHost())
			return;

		auto szMessage = PacketReadString(pData, iMessageLength, &iReadCount);

		printf("%s\n", szMessage);

		bool IsGranted = false;

		if (!strcmp(szMessage, ADMIN_GRANTED))
		{
			printf("[+] %s() -> Server allowed admin\n", __FUNCTION__);
			this->m_bIsAdmin = true;
		}
		else 
			printf("[+] %s() -> Server disallowed admin\n", __FUNCTION__);

		this->m_pChatData->SendNewMessage(szAuthorName, szMessage);
	}
	else if (Msg == DELETE_MSG)
	{
		if (IsHost() && !this->m_vUsersList[Packet.m_iConnectionID].m_bIsAdmin)
		{
			printf("[+] %s() -> User %d not admin\n", __FUNCTION__, Packet.m_iConnectionID);
			return;
		}

		auto iArraySize = *(int*)PacketReadInteger(pData, &iReadCount);

		std::vector<int> vToDeleteMessages;

		printf("[+] %s() -> Messages for delete array size: %d\n", __FUNCTION__, iArraySize);
		printf("[+] %s() -> Numbers: ", __FUNCTION__);

		for (auto i = 0; i < iArraySize; i++)
		{
			auto iMessageID = *(int*)(pData + iReadCount);
			printf("%d ", i);
			vToDeleteMessages.push_back(iMessageID);
			iReadCount += sizeof(int);
		}

		printf("\n");
		
		for (auto& i : vToDeleteMessages)
			this->m_pChatData->DeleteMessage(i);

		std::vector<int> vExcludeId{ Packet.m_iConnectionID }; 
		this->m_pNetwork->SendPacketExcludeID(Packet.m_pPacket, Packet.m_iSize, &vExcludeId);
	}
	else 
	{
		printf("[+] %s() -> Not valid msg\n", __FUNCTION__);
	}
}

int* CNetworkChatManager::PacketReadInteger(char* pData, int* pReadCount)
{
	auto ret = pData + *pReadCount;
	*pReadCount += sizeof(int);
	return (int*)ret;
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

	iStrLength = *(int*)PacketReadInteger(pData, pReadCount);

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

void CNetworkChatManager::PacketWriteString(char* pData, int* pWriteCount, char* szValue, int iValueLength)
{
	memcpy(pData + *pWriteCount, szValue, iValueLength);
	*pWriteCount += iValueLength;

	*(pData + *pWriteCount) = NULL_TERMINATE_BYTE;
	*pWriteCount += NULL_TERMINATE_BYTE_SIZE;
}

void CNetworkChatManager::PacketWriteNetString(char* pData, int* pWriteCount, char* szValue, int iValueLength)
{
	PacketWriteInteger(pData, pWriteCount, iValueLength);
	PacketWriteString(pData, pWriteCount, szValue, iValueLength);
}

int CNetworkChatManager::CalcNetString(int iValueLength)
{
	return sizeof(int) + iValueLength + NULL_TERMINATE_BYTE_SIZE;
}

MSG_TYPE CNetworkChatManager::PacketReadMsgType(char* pData, int* pReadCount)
{
	auto szMsgType = PacketReadNetString(pData, pReadCount);
	return GetMsgType(szMsgType);
}

bool CNetworkChatManager::SendChatMessage(char* szMessage)
{
	auto iMessageLength = strlen(szMessage);

	if (!iMessageLength)
		return false;

	if (IsHost())
	{
		this->m_iMessageCount++;
		this->m_pChatData->SendNewMessage(this->m_szUsername, szMessage, this->m_iMessageCount);
	}
		
	return SendNetMsg(MSG_TYPE::CHAT_MSG, this->m_szUsername, szMessage, iMessageLength, this->m_iMessageCount);
}

bool CNetworkChatManager::RequestAdmin(char* szLogin, char* szPassword)
{
	if (!szLogin || !szPassword)
		return false;

	int szLoginLength = strlen(szLogin);
	int szPasswordLength = strlen(szPassword);

	if (szLoginLength < 2 || szPasswordLength < 2)
		return false;

	int MsgSize = CalcNetString(szLoginLength) + CalcNetString(szPasswordLength);

	char* buff = new char[MsgSize]();

	int iWriteCount = 0;

	PacketWriteNetString(buff, &iWriteCount, szLogin, szLoginLength);

	PacketWriteNetString(buff, &iWriteCount, szPassword, szPasswordLength);

	SendNetMsg(MSG_TYPE::ADMIN_REQUEST, this->m_szUsername, (char*)buff, MsgSize);

	delete[] buff;

	return true;
}

bool CNetworkChatManager::SendStatusAdmin(int ID, bool IsGranted)
{
	auto Status = IsGranted ? ADMIN_GRANTED : ADMIN_NOT_GRANTED;

	std::vector<int> List{ ID };

	return SendNetMsg(MSG_TYPE::ADMIN_STATUS, (char*)"SYSTEM", (char*)Status, strlen(Status), UNTRACKED_MESSAGE, &List);
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

	auto ret = SendNetMsg(MSG_TYPE::DELETE_MSG, this->m_szUsername, buff, MsgSize);

	delete[] buff;

	if (ret)
	{
		for (auto& i : *MsgsList)
			this->m_pChatData->DeleteMessage(i);
	}

	return ret;
}

bool CNetworkChatManager::SendNetMsg(MSG_TYPE MsgType, char* szAuthor, char* szMessage, int iMessageSize, int iMessageID, std::vector<int>* IDList)
{
	if (!this->m_bIsInitialized)
		return false;

	if (iMessageSize < 1)
		return false;

	char* szMsgType = nullptr;

	switch (MsgType)
	{
	case CHAT_MSG:
		szMsgType = (char*)CHAT_MESSAGE_MSG;
		break;
	case ADMIN_REQUEST:
		szMsgType = (char*)ADMIN_REQUEST_MSG;
		break;
	case ADMIN_STATUS:
		szMsgType = (char*)ADMIN_STATUS_MSG;
		break;
	case DELETE_MSG:
		szMsgType = (char*)DELETE_CHAT_MSG;
		break;
	default:
		return false;
	}

	if (!szMsgType)
		return false;

	auto IsChatMessage = MsgType == CHAT_MSG;

	int iMsgTypeSize = strlen(szMsgType);
	int iUsernameSize = strlen(szAuthor);

	int iPacketSize = CalcNetString(iMsgTypeSize) + CalcNetString(iUsernameSize) + CalcNetString(iMessageSize);

	if (IsChatMessage)
		iPacketSize += sizeof(int);

	auto pPacket = new char[iPacketSize];

	int iWriteStep = 0;

	//Msg type
	PacketWriteNetString(pPacket, &iWriteStep, szMsgType, iMsgTypeSize);

	//Username
	PacketWriteNetString(pPacket, &iWriteStep, szAuthor, iUsernameSize);
	
	//Msg
	PacketWriteNetString(pPacket, &iWriteStep, szMessage, iMessageSize);

	if (IsChatMessage)
		PacketWriteInteger(pPacket, &iWriteStep, iMessageID);

	printf("[+] %s() -> iPacketSize: %d, iWriteStep: %d\n", __FUNCTION__, iPacketSize, iWriteStep);

	auto bSendStatus = false;

	bSendStatus = !IDList ? 
		this->m_pNetwork->SendPacketAll(pPacket, iPacketSize) :
		this->m_pNetwork->SendPacketIncludeID(pPacket, iPacketSize, IDList);

	delete[] pPacket;

	return bSendStatus;
}

bool CNetworkChatManager::SendLocalMsg(char* szAuthor, char* szMessage, int iMessageID)
{
	return this->m_pChatData->SendNewMessage(szAuthor, szMessage, iMessageID);
}

CChatData* CNetworkChatManager::GetChatData()
{
	return this->m_pChatData;
}

size_t CNetworkChatManager::GetChatArraySize()
{
	return this->m_pChatData->GetMessagesArraySize();
}

bool CNetworkChatManager::IsNeedExit()
{
	return this->m_bNeedExit;
}

bool CNetworkChatManager::IsHost()
{
	return this->m_bIsHost && this->m_pNetwork->IsHost();
}

MSG_TYPE CNetworkChatManager::GetMsgType(char* szMsg)
{
	MSG_TYPE ret = MSG_TYPE::NONE_MSG;

	if (!strcmp(szMsg, ADMIN_REQUEST_MSG))
		ret = ADMIN_REQUEST;
	if (!strcmp(szMsg, ADMIN_STATUS_MSG))
		ret = ADMIN_STATUS;
	else if (!strcmp(szMsg, DELETE_CHAT_MSG))
		ret = DELETE_MSG;
	else if (!strcmp(szMsg, CHAT_MESSAGE_MSG))
		ret = CHAT_MSG;

	return ret;
}

bool CNetworkChatManager::GrantAdmin(char* szLogin, char* szPassword, int iConnectionID)
{
	if (!strcmp(szLogin, "Admin") && !strcmp(szPassword, "Admin"))
	{
		auto User = &this->m_vUsersList[iConnectionID];

		if (!User->m_bIsInitialized)
			return false;

		User->m_bIsAdmin = true;

		return true;
	}

	return false;
}

void CNetworkChatManager::AddUser(int ID, int IP, int iPort)
{
	chat_user user{};
	user.m_bIsInitialized = true;
	user.m_IP = IP;
	user.m_iPort = iPort;
	user.m_bIsAdmin = false;
	this->m_vUsersList[ID] = user;
}

void CNetworkChatManager::ResendLastMessagesToClient(int ID, int iNumberToResend)
{
	printf("[+] %s() -> Resend last %d messages to %d client\n", __FUNCTION__, iNumberToResend, ID);

	std::vector<int> IDList{ ID };

	int i = 0;
	for (auto it = GetChatData()->Begin(); it != GetChatData()->End() && i < iNumberToResend; it++, i++)
	{
		auto message = *it;
		SendNetMsg(MSG_TYPE::CHAT_MSG, message->m_szUsername, message->m_szMessage, strlen(message->m_szMessage), message->m_iMessageID, &IDList);
	}
}

bool CNetworkChatManager::IsAdmin()
{
	return this->m_bIsAdmin;
}