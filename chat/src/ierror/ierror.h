#pragma once

class IError
{
private:
	char m_szWhat[1024 + 1];
public:
	IError();
	~IError();

	const char* What();
protected:
	void SetError(const char* szErrorText, ...);
};