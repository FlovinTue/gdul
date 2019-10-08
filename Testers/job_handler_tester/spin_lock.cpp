#include "spin_lock.h"
#include <thread>

spin_lock::spin_lock()
	: myEntryCounter(0)
	, myExitCounter(0)
{
}

spin_lock::~spin_lock()
{
}

void spin_lock::lock()
{
	uint16_t const ticket(myEntryCounter.fetch_add(1, std::memory_order_acquire));

	while (myExitCounter.load(std::memory_order_acquire) != ticket) {
		std::this_thread::yield();
	}
}

void spin_lock::unlock()
{
	myExitCounter.fetch_add(1, std::memory_order_release);
}
