// ThreadSafeConsumptionQueue.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <string>
#include <vector>
#include <iostream>


#define GDUL
//#define GDUL_FIFO
//#define MOODYCAMEL
//#define MSC_RUNTIME
//#define MTX_WRAPPER	

#include "Tester.h"
#include <string>
#include <random>
#include <functional>
#include "../Common/tracking_allocator.h"

//#include <vld.h>
#include <mutex>


#pragma warning(disable : 4324)

namespace gdul
{
std::random_device rd;
std::mt19937 rng(rd());
}

class Thrower
{
public:
#ifdef GDUL
	Thrower() = default;
	Thrower(std::uint32_t aCount) : count(aCount) {}
	Thrower& operator=(Thrower&&) = default;
	Thrower& operator=(const Thrower& aOther)
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
	Thrower& operator+=(std::uint32_t aCount)
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
	gdul::tracking_allocator<std::uint8_t> alloc;
	//std::allocator<uint8_t> alloc;

	for (std::uint32_t i = 0; i < 40; ++i) {
#ifdef CQ_ENABLE_EXCEPTIONHANDLING
#ifdef NDEBUG
		uint32_t runs(1000);
#else
		uint32_t runs(100);
#endif
#else
#ifdef NDEBUG
		uint32_t runs(1000);
#else
		uint32_t runs(100);
#endif
#endif

		gdul::queue_testrun<Thrower>(runs, alloc);
	}


	std::cout << "\nAfter runs finished, the allocated value is: " << gdul::s_allocated << '\n' << std::endl;

	gdul::doPrint = true;

	return 0;
}

