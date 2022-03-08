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

	bool SendNewMessage(char* szUsername, char* szMessage, int iMessageCount = UNTRACKED_MESSAGE, size_t UsernameSize = 0, size_t MessageSize = 0);
	bool SendNewMessage(char* szData, int iMessageStartAfter, int iMessageCount, size_t DataSize = 0);
	ppArrayMessages GetChat();
	size_t GetMessagesArraySize();
	void CleanupData();
	void DeleteMessage(int iMessageID);

	using Iterator = CIterator<pMessage>;
	Iterator Begin();
	Iterator End();
private:
	std::uint8_t GetPointerSize();
	pMessage AllocateNewMessagePointer(size_t SizeUsername, size_t SizeMessage);
	void AddMessage(char* szUsername, size_t UsernameSize, char* szMessage, size_t MessageSize, int iMessageCount, pMessage Message);
	void IncreaseMessagesCounter();

	ppArrayMessages m_ppMessagesArray;
	size_t m_mMessagesArraySize;
};
