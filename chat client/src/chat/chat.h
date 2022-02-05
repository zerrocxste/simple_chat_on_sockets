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

	bool SendNewMessage(char* szusername, char* szmessage, size_t username_size = 0, size_t message_size = 0);
	bool SendNewMessage(char* szdata, int message_start_after, size_t data_size = 0);
	ppArrayMessages GetChat();
	size_t GetMessagesArraySize();
	void CleanupData();

	using Iterator = CIterator<pMessage>;
	Iterator Begin();
	Iterator End();
private:
	std::uint8_t GetPointerSize();
	pMessage AllocateNewMessagePointer(size_t size_username, size_t size_message);
	void AddMessage(char* szusername, size_t username_size, char* szmessage, size_t message_size, pMessage message);
	void IncreaseMessagesCounter();

	ppArrayMessages m_ppMessagesArray;
	size_t m_mMessagesArraySize;
};
