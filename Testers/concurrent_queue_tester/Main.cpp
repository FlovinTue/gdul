// ThreadSafeConsumptionQueue.cpp : Defines the entry point for the console application.
//



#include "stdafx.h"
#define GDUL
//#define GDUL_CPQ
//#define GDUL_FIFO
//#define MOODYCAMEL
//#define MSC_RUNTIME
//#define MTX_WRAPPER	
//#define RIGTORP

#include <gdul/containers/concurrent_queue.h>
#include "../queue_tester/tester.h"
#include <random>
#include "../Common/tracking_allocator.h"

#include <vld.h>


#pragma warning(disable : 4324)

namespace gdul {
std::random_device rd;
std::mt19937 rng(rd());
}

class thrower
{
public:
#ifdef GDUL
	thrower() = default;
	thrower(std::uint32_t aCount) : count(aCount) {}
	thrower& operator=(thrower&&) = default;
	thrower& operator=(const thrower& aOther)
	{
		count = aOther.count;
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
		throwTarget = aOther.throwTarget;
		if (!--throwTarget) {
			throwTarget = gdul::rng() % 50000;
			throw std::runtime_error("Test");
		}
#endif
		return *this;
	}
	thrower& operator+=(std::uint32_t aCount)
	{
		count += aCount;
		return *this;
	}
#endif

	uint32_t count = 0;
#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	uint32_t throwTarget = gdul::rng() % 50000;
#endif
};

int main()
{
	gdul::concurrent_queue<int> q;
	q.push(1);
	q.push(2);
	q.push(3);


	int out1(0);
	int out2(0);
	int out3(0);

	const bool res1(q.try_pop(out1));
	const bool res2(q.try_pop(out2));
	const bool res3(q.try_pop(out3));

	gdul::tracking_allocator<std::uint8_t> alloc;
	//std::allocator<uint8_t> alloc;


	gdul::queue_testrun<thrower>(103333, alloc);

	std::cout << "\nAfter runs finished, the allocated value is: " << gdul::s_allocated << '\n' << std::endl;

	gdul::doPrint = true;

	return 0;
}

