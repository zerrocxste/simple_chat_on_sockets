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
	return this->m_Pointer;
}