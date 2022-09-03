#include "../../includes.h"

CChatData::CChatData()
{
	TRACE_FUNC("Contructor called\n");

	this->m_ppMessagesArray = 0;
	this->m_mMessagesArraySize = 0;
}

CChatData::~CChatData()
{
	TRACE_FUNC("Destructor called\n");

	CleanupData();
}

bool CChatData::SendNewMessage(char* szUsername, char* szMessage, netconnectcount iMessageOwnerID, int iMessageCount, bool IsMessageImportant, std::size_t UsernameSize, std::size_t MessageSize)
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

	AddMessage(szUsername, UsernameSize, szMessage, MessageSize, iMessageOwnerID, iMessageCount, IsMessageImportant, MsgObj);

	IncreaseMessagesCounter();

	return true;
}

ppArrayMessages CChatData::GetChat()
{
	return this->m_ppMessagesArray;
}

std::size_t CChatData::GetMessagesArraySize()
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

CChatData::Iterator CChatData::At(std::size_t c)
{
	return this->m_ppMessagesArray + c;
}

std::uint8_t CChatData::GetPointerSize()
{
	return sizeof(std::uintptr_t);
}

pMessage CChatData::AllocateNewMessagePointer(std::size_t SizeUsername, std::size_t SizeMessage)
{
	auto PointerSize = GetPointerSize();

	this->m_ppMessagesArray = (ppArrayMessages)realloc(this->m_ppMessagesArray, (this->m_mMessagesArraySize * PointerSize) + PointerSize);

	this->m_ppMessagesArray[this->m_mMessagesArraySize] = (pMessage)realloc(nullptr, sizeof(Message));

	this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szUsername = (char*)realloc(nullptr, SizeUsername + 1);
	memset(this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szUsername, 0, SizeUsername + 1);

	this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szMessage = (char*)realloc(nullptr, SizeMessage + 1);
	memset(this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szMessage, 0, SizeMessage + 1);

	TRACE_FUNC("Allocated memory for message packet. packet va: %p, username va: %p, message va: %p\n",
		this->m_ppMessagesArray[this->m_mMessagesArraySize],
		this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szUsername,
		this->m_ppMessagesArray[this->m_mMessagesArraySize]->m_szMessage);

	return this->m_ppMessagesArray[this->m_mMessagesArraySize];
}

void CChatData::AddMessage(char* szUsername, std::size_t UsernameSize, char* szMessage, std::size_t MessageSize, netconnectcount iMessageOwnerID, int iMessageCount, bool IsMessageImportant, pMessage Message)
{
	Message->m_iMessageOwnerID = iMessageOwnerID;
	Message->m_bMessageIsImportant = IsMessageImportant;
	Message->m_iMessageID = iMessageCount;
	memcpy(Message->m_szUsername, szUsername, UsernameSize);
	memcpy(Message->m_szMessage, szMessage, MessageSize);
}

void CChatData::DeleteMessage(int iMessageID)
{
	for (auto i = 0; i < GetMessagesArraySize(); i++)
	{
		auto& message = this->m_ppMessagesArray[i];

		if (!message)
			continue;

		if (message->m_iMessageID == iMessageID)
		{
			free(message->m_szUsername);
			message->m_szUsername = nullptr;

			free(message->m_szMessage);
			message->m_szMessage = nullptr;

			free(message);
			message = nullptr;

			break;
		}
	}
}

void CChatData::IncreaseMessagesCounter()
{
	this->m_mMessagesArraySize++;
}

void CChatData::CleanupData()
{
	for (auto i = 0; i < GetMessagesArraySize(); i++)
	{
		auto& message = this->m_ppMessagesArray[i];

		if (!message)
			continue;

		free(message->m_szUsername);
		message->m_szUsername = nullptr;

		free(message->m_szMessage);
		message->m_szMessage = nullptr;

		free(message);
		message = nullptr;
	}

	free(this->m_ppMessagesArray);
	this->m_ppMessagesArray = nullptr;
	this->m_mMessagesArraySize = 0;
}