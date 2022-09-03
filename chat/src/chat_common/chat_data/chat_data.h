struct Message
{
	netconnectcount m_iMessageOwnerID;
	bool m_bMessageIsImportant;
	int m_iMessageID;
	char* m_szUsername;
	char* m_szMessage;
};
typedef Message* pMessage;
typedef pMessage* ppArrayMessages;

template <class T>
class CIterator
{
public:
	CIterator(void* ptr);
	~CIterator();

	CIterator<T>* operator++(int);
	bool operator<(CIterator o);
	bool operator>(CIterator o);
	bool operator!=(CIterator o);
	T operator*();
private:
	T m_Pointer;
};

template <class T>
CIterator<T>::CIterator(void* ptr)
{
	this->m_Pointer = (T)ptr;
}

template <class T>
CIterator<T>::~CIterator()
{

}

template <class T>
CIterator<T>* CIterator<T>::operator++(int)
{
	std::uintptr_t* ptr = (std::uintptr_t*)this->m_Pointer;
	ptr++;
	this->m_Pointer = (T)ptr;
	return this;
}

template <class T>
bool CIterator<T>::operator<(CIterator o)
{
	return this->m_Pointer < o.m_Pointer;
}

template <class T>
bool CIterator<T>::operator>(CIterator o)
{
	return this->m_Pointer > o.m_Pointer;
}

template <class T>
bool CIterator<T>::operator!=(CIterator o)
{
	return this->m_Pointer != o.m_Pointer;
}

template <class T>
T CIterator<T>::operator*()
{
	return (T)(*(T*)(this->m_Pointer));
}

class CChatData
{
public:
	CChatData();
	~CChatData();

	bool SendNewMessage(char* szUsername, char* szMessage, netconnectcount iMessageOwnerID, int iMessageCount = UNTRACKED_MESSAGE, bool IsMessageImportant = true, std::size_t UsernameSize = 0, std::size_t MessageSize = 0);
	ppArrayMessages GetChat();
	std::size_t GetMessagesArraySize();
	void CleanupData();
	void DeleteMessage(int iMessageID);

	using Iterator = CIterator<pMessage>;
	Iterator Begin();
	Iterator End();
	Iterator At(std::size_t c);
private:
	std::uint8_t GetPointerSize();
	pMessage AllocateNewMessagePointer(std::size_t SizeUsername, std::size_t SizeMessage);
	void AddMessage(char* szUsername, std::size_t UsernameSize, char* szMessage, std::size_t MessageSize, netconnectcount iMessageOwnerID, int iMessageCount, bool IsMessageImportant, pMessage Message);
	void IncreaseMessagesCounter();

	ppArrayMessages m_ppMessagesArray;
	std::size_t m_mMessagesArraySize;
};
