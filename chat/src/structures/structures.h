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