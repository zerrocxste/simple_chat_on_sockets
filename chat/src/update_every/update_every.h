#pragma once

#include <iostream>
#include <Windows.h>

class CUpdateEvery
{
	std::uint32_t m_ToUpdateMSCounter;
	std::uint64_t m_CurrentCounter;

	std::uint64_t GetTimeTick();
public:
	CUpdateEvery(std::uint32_t MillisecondToUpdate);

	void SetUpdateFor(std::uint32_t MillisecondToUpdate);

	void Update(void(*funcToUpdate)());
};