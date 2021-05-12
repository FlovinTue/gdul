#pragma warning(disable:4530)

#include <vld.h>
#include <random>

#include <gdul/WIP/concurrent_queue_fifo_v10.h>

#define MSC_RUNTIME

#include <gdul/../../Testers/Common/tracking_allocator.h>
#include <gdul/../../Testers/queue_tester/tester.h>


#pragma warning(disable : 4324)

namespace gdul {
std::random_device rd;
std::mt19937 rng(rd());
}


int main()
{
	gdul::qsbr::register_thread();

	gdul::concurrent_queue_fifo<uint32_t> que;

	for (uint32_t i = 0; i < 4; ++i) {
		que.push(i + 1);
	}

	for (uint32_t i = 0; i < 4; ++i) {
		uint32_t out;
		const bool result = que.try_pop(out);
		assert(result && "should have succeeded");
		assert(out == i + 1 && "bad out data");
	}

	for (uint32_t i = 0; i < 4; ++i) {
		que.push(i + 1);
	}

	for (uint32_t i = 0; i < 4; ++i) {
		uint32_t out;
		const bool result = que.try_pop(out);
		assert(result && "Should have succeeded");
		assert(out == i + 1 && "Bad out data");
	}

	gdul::tracking_allocator<std::uint8_t> alloc;
	gdul::queue_testrun<uint32_t>(10000, alloc);


	std::cout << "\nAfter runs finished, the allocated value is: " << gdul::s_allocated << '\n' << std::endl;

	gdul::doPrint = true;

	return 0;
}
