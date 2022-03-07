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

bool CChatData::SendNewMessage(char* szUsername, char* szMessage, int iMessageCount, size_t UsernameSize, size_t MessageSize)
{
	if (!szUsername || !szMessage)
		return false;

	if (!UsernameSize)
		UsernameSize = strlen(szUsername);

	if (!MessageSize)
		MessageSize = strlen(szMessage);

	if (!UsernameSize || !MessageSize)
		return false;

	auto MsgObj = AllocateNewMessagePointer(UsernameSize, MessageSize);

	AddMessage(szUsername, UsernameSize, szMessage, MessageSize, iMessageCount, MsgObj);

	IncreaseMessagesCounter();

	return true;
}

bool CChatData::SendNewMessage(char* szData, int iMessageStartAfter, int iMessageCount, size_t DataSize)
{
	if (!szData)
		return false;

	if (!DataSize)
		DataSize = strlen(szData);

	if (!DataSize)
		return false;

	auto szUsername = szData;
	auto UsernameSize = strlen(szData);

	auto szMessage = szData + iMessageStartAfter;
	auto MessageSize = strlen(szMessage);

	auto MsgObj = AllocateNewMessagePointer(UsernameSize, MessageSize);

	AddMessage(szUsername, UsernameSize, szMessage, MessageSize, iMessageCount, MsgObj);

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

pMessage CChatData::AllocateNewMessagePointer(size_t SizeUsername, size_t SizeMessage)
{
	auto PointerSize = GetPointerSize();

	this->m_ppMessagesArray = (ppArrayMessages)realloc(this->m_ppMessagesArray, (this->m_mMessagesArraySize * PointerSize) + PointerSize);

	this->m_ppMessagesArray[this->m_mMessagesArraySize] = (pMessage)realloc(nullptr, sizeof(Message));

	this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szUsername = (char*)realloc(nullptr, SizeUsername + 1);
	memset(this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szUsername, 0, SizeUsername + 1);

	this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szMessage = (char*)realloc(nullptr, SizeMessage + 1);
	memset(this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szMessage, 0, SizeMessage + 1);

	printf("[+] %s. Allocated memory for message packet. packet va: %p, username va: %p, message va: %p\n", __FUNCTION__,
		this->m_ppMessagesArray[this->m_mMessagesArraySize],
		this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szUsername,
		this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szMessage);

	return this->m_ppMessagesArray[this->m_mMessagesArraySize];
}

void CChatData::AddMessage(char* szUsername, size_t UsernameSize, char* szMessage, size_t MessageSize, int iMessageCount, pMessage Message)
{
	Message->m_iMessageID = iMessageCount;
	memcpy(Message->m_szUsername, szUsername, UsernameSize);
	memcpy(Message->m_szMessage, szMessage, MessageSize);
}

void CChatData::IncreaseMessagesCounter()
{
	this->m_mMessagesArraySize++;
}

void CChatData::CleanupData()
{
	for (auto it = Begin(); it < End(); it++)
	{
		auto message = *it;

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