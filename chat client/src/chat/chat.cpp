#include "../includes.h"

CChatData::CChatData()
{
	printf("[+] %s -> Contructor called\n", __FUNCTION__);
	this->m_ppMessagesArray = 0;
	this->m_mMessagesArraySize = 0;
}

CChatData::~CChatData()
{
	printf("[+] %s -> Destructor called\n", __FUNCTION__);
	CleanupData();
}

bool CChatData::SendNewMessage(char* szusername, char* szmessage, size_t username_size, size_t message_size)
{
	if (!szusername || !szmessage)
		return false;

	if (!username_size)
		username_size = strlen(szusername);

	if (!message_size)
		message_size = strlen(szmessage);

	if (!username_size || !message_size)
		return false;

	auto msg_obj = AllocateNewMessagePointer(username_size, message_size);

	AddMessage(szusername, username_size, szmessage, message_size, msg_obj);

	IncreaseMessagesCounter();
	
	return true;
}

bool CChatData::SendNewMessage(char* szdata, int message_start_after, size_t data_size)
{
	if (!szdata)
		return false;

	if (!data_size)
		data_size = strlen(szdata);

	if (!data_size)
		return false;

	auto szusername = szdata;
	auto username_size = strlen(szdata);

	auto szmessage = szdata + message_start_after;
	auto message_size = strlen(szmessage);

	auto msg_obj = AllocateNewMessagePointer(username_size, message_size);

	AddMessage(szusername, username_size, szmessage, message_size, msg_obj);

	IncreaseMessagesCounter();
	
	return true;
}

ppArrayMessages CChatData::GetChat()
{
	return this->m_ppMessagesArray;
}

size_t CChatData::GetMessagesArraySize()
{
	return this->m_mMessagesArraySize;
}

CChatData::Iterator CChatData::Begin()
{
	return this->m_ppMessagesArray;
}

CChatData::Iterator CChatData::End()
{
	return this->m_ppMessagesArray + this->m_mMessagesArraySize;
}

std::uint8_t CChatData::GetPointerSize()
{
	return sizeof(std::uintptr_t);
}

pMessage CChatData::AllocateNewMessagePointer(size_t size_username, size_t size_message)
{
	auto PointerSize = GetPointerSize();

	this->m_ppMessagesArray = (ppArrayMessages)realloc(this->m_ppMessagesArray, (this->m_mMessagesArraySize * PointerSize) + PointerSize);

	this->m_ppMessagesArray[this->m_mMessagesArraySize] = (pMessage)realloc(NULL, sizeof(Message));

	this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szUsername = (char*)realloc(NULL, size_username + 1);
	memset(this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szUsername, 0, size_username + 1);

	this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szMessage = (char*)realloc(NULL, size_message + 1);
	memset(this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szMessage, 0, size_message + 1);

	printf("[+] %s. Allocated memory for message packet. packet va: %p, username va: %p, message va: %p\n", __FUNCTION__,
		this->m_ppMessagesArray[this->m_mMessagesArraySize],
		this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szUsername,
		this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szMessage);

	return this->m_ppMessagesArray[this->m_mMessagesArraySize];
}

void CChatData::AddMessage(char* szusername, size_t username_size, char* szmessage, size_t message_size, pMessage message)
{
	memcpy(message->m_szUsername, szusername, username_size);
	memcpy(message->m_szMessage, szmessage, message_size);
}

void CChatData::IncreaseMessagesCounter()
{
	this->m_mMessagesArraySize++;
}

void CChatData::CleanupData()
{
	for (auto it = Begin(); it < End(); it++)
	{
		auto message = **it;

		if (!message)
			continue;

		free(message->m_szUsername);
		free(message->m_szMessage);
		free(message);
	}

	free(this->m_ppMessagesArray);
	this->m_ppMessagesArray = nullptr;
	this->m_mMessagesArraySize = 0;
}