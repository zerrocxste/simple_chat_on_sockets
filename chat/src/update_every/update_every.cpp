#include "update_every.h"

CUpdateEvery::CUpdateEvery(std::uint32_t MillisecondToUpdate)
{
	SetUpdateFor(MillisecondToUpdate);
}

void CUpdateEvery::SetUpdateFor(std::uint32_t MillisecondToUpdate)
{
	this->m_ToUpdateMSCounter = MillisecondToUpdate;
}

void CUpdateEvery::Update(void(*funcToUpdate)())
{
	auto TimeTick = GetTimeTick();

	if (TimeTick - this->m_CurrentCounter > this->m_ToUpdateMSCounter)
	{
		this->m_CurrentCounter = TimeTick;
		funcToUpdate();
	}
}

std::uint64_t CUpdateEvery::GetTimeTick()
{
	return GetTickCount64();
}