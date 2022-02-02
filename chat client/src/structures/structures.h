#pragma once

struct net_packet
{
	net_packet() : m_pPacket(nullptr), m_iSize(0) {};
	net_packet(void* pPacket, int iSize) : m_pPacket(pPacket), m_iSize(iSize) {}
	void* m_pPacket;
	int m_iSize;
};

struct ban_user
{
	int m_Id;
	//std::chrono
};

struct Message
{
	char* m_szUsername;
	char* m_szMessage;
};
typedef Message* pMessage;
typedef pMessage* ppArrayMessages;

struct network_thread_arg
{
	bool bIsHost;
	char szUsername[32];
};

constexpr auto NULL_TERMINATE_BYTE = '\0';
constexpr auto NULL_TERMINATE_BYTE_SIZE = 1;

using cbpReceivePacket = void(*)(net_packet);

enum APP_MODE
{
	NOT_INITIALIZED,
	PROCESS_INITIALIZING,
	FAILED_INITIALIZING,
	HOST,
	CLIENT
};

constexpr auto MAX_USERNAME_SIZE = 32;
constexpr auto MAX_MESSAGE_TEXT_SIZE = 4096;

constexpr auto CLIENT_SOCKET = 0;

constexpr auto MAX_PROCESSED_USERS_IN_CHAT = 10;
