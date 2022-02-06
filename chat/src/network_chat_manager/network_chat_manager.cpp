#include "../includes.h"

CNetworkChatManager::CNetworkChatManager(bool IsHost, char* szUsername, char* pszIP, int iPort, int iMaxProcessedUsersNumber) :
	m_bIsInitialized(false),
	m_bIsHost(IsHost),
	m_bNeedExit(false),
	m_iMaxProcessedUsersNumber(iMaxProcessedUsersNumber)
{
	printf("[+] %s -> Contructor called\n", __FUNCTION__);

	memcpy(this->m_szUsername, szUsername, strlen(szUsername) + NULL_TERMINATE_BYTE_SIZE);
	this->m_pNetwork = new CNetwork(this->m_bIsHost, pszIP, iPort, m_iMaxProcessedUsersNumber);
	this->m_pChatData = new CChatData();
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

	if (Packet.m_iSize)
	{
		printf("[+] %s -> Packet.m_pPacket: %p, Packet.m_iSize: %d\n", __FUNCTION__, Packet.m_pPacket, Packet.m_iSize);

		constexpr auto iAuthorNameLengthInfo = sizeof(int);

		auto iAuthorNameLength = *(int*)Packet.m_pPacket;
		auto iStringDataStart = (char*)Packet.m_pPacket + iAuthorNameLengthInfo;
		auto szAuthorName = iStringDataStart;
		auto szMessage = iStringDataStart + iAuthorNameLength + NULL_TERMINATE_BYTE_SIZE;

		this->m_pChatData->SendNewMessage(szAuthorName, szMessage);
	}
}

bool CNetworkChatManager::SendNewMessage(char* szauthor, char* szmessage)
{
	if (!this->m_bIsInitialized)
		return false;

	int iUsernameSize = strlen(szauthor);
	int iMessageSize = strlen(szmessage);

	int iPacketSize = sizeof(int) + iUsernameSize + NULL_TERMINATE_BYTE_SIZE + iMessageSize + NULL_TERMINATE_BYTE_SIZE;

	auto pPacket = new char[iPacketSize];

	memcpy(pPacket, &iUsernameSize, sizeof(int));

	memcpy(pPacket + sizeof(int), szauthor, iUsernameSize);
	*(pPacket + sizeof(int) + iUsernameSize) = NULL_TERMINATE_BYTE;
	memcpy(pPacket + sizeof(int) + iUsernameSize + NULL_TERMINATE_BYTE_SIZE, szmessage, iMessageSize);
	*(pPacket + iPacketSize - NULL_TERMINATE_BYTE_SIZE) = NULL_TERMINATE_BYTE;

	auto bSendStatus = this->m_pNetwork->SendPacket(pPacket, iPacketSize);

	delete[] pPacket;

	if (!bSendStatus)
		return false;

	this->m_pChatData->SendNewMessage(szauthor, szmessage);

	return true;
}

bool CNetworkChatManager::SendNewMessage(char* szmessage)
{
	if (!this->m_bIsInitialized)
		return false;

	int iUsernameSize = strlen(this->m_szUsername);
	int iMessageSize = strlen(szmessage);

	int iPacketSize = sizeof(int) + iUsernameSize + NULL_TERMINATE_BYTE_SIZE + iMessageSize + NULL_TERMINATE_BYTE_SIZE;

	auto pPacket = new char[iPacketSize];

	memcpy(pPacket, &iUsernameSize, sizeof(int));

	memcpy(pPacket + sizeof(int), this->m_szUsername, iUsernameSize);
	*(pPacket + sizeof(int) + iUsernameSize) = NULL_TERMINATE_BYTE;

	memcpy(pPacket + sizeof(int) + iUsernameSize + NULL_TERMINATE_BYTE_SIZE, szmessage, iMessageSize);
	*(pPacket + iPacketSize - NULL_TERMINATE_BYTE_SIZE) = NULL_TERMINATE_BYTE;

	auto bSendStatus = this->m_pNetwork->SendPacket(pPacket, iPacketSize);

	delete[] pPacket;

	if (!bSendStatus)
		return false;

	this->m_pChatData->SendNewMessage(this->m_szUsername, szmessage);

	return true;
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
	return this->m_bIsHost;
}
