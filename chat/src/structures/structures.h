#pragma once

struct net_packet
{
	net_packet() : 
		m_pPacket(nullptr), 
		m_iSize(0),
		m_iConnectionID(0) 
	{

	};

	net_packet(void* pPacket, int iSize, int iConnectionID) : 
		m_pPacket(pPacket),
		m_iSize(iSize), 
		m_iConnectionID(iConnectionID) 
	{

	}

	void* m_pPacket;
	int m_iSize;
	int m_iConnectionID;
};

struct Message
{
	int m_iMessageID;
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

struct chat_user
{
	bool m_bConnected;
	int m_IP;
	int m_iPort;
	bool m_bIsAdmin;
	char m_szUsername[MAX_USERNAME_SIZE];
};