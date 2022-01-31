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

	using Iterator = CIterator<ppArrayMessages>;
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
