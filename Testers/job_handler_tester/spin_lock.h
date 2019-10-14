#pragma once

#include <atomic>

#pragma warning(push)
#pragma warning(disable : 4324)
class spin_lock
{
public:
	spin_lock();
	~spin_lock();

	void lock();
	void unlock();

private:
	alignas(64) std::atomic<std::uint16_t> myEntryCounter;
	alignas(64) std::atomic<std::uint16_t> myExitCounter;
};
#pragma warning(pop)