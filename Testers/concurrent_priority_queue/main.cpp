// concurrent_priority_queue.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <gdul/concurrent_priority_queue/concurrent_priority_queue.h>

#define GDUL_CPQ
//#define MS_CPQ
#include "../queue_tester/tester.h"

namespace gdul {
std::random_device rd;
std::mt19937 rng(rd());
}



int main()
{
	std::allocator<std::uint8_t> alloc;
	gdul::concurrent_priority_queue<int, float> q(alloc);
	std::pair<int, float> two	(std::make_pair(2, 1.f));
	std::pair<int, float> six	(std::make_pair(6, 1.f));
	std::pair<int, float> three	(std::make_pair(3, 1.f));
	std::pair<int, float> four	(std::make_pair(4, 1.f));
	std::pair<int, float> one	(std::make_pair(1, 1.f));
	std::pair<int, float> five	(std::make_pair(5, 1.f));

	q.push(two);
	q.push(six);
	q.push(three);
	q.push(four);
	q.push(one);
	q.push(five);
	q.push(five);
	q.push(five);
	q.push(five);
	q.push(five);

	decltype(q)::iterator begin(q.unsafe_begin());
	decltype(q)::iterator end(q.unsafe_end());
	decltype(q)::iterator findResult(q.unsafe_find(2));

	assert(findResult->first == 2 && "Unexpected find value");
	
	std::pair<int, float> outone;
	std::pair<int, float> outtwo;
	std::pair<int, float> outthree;
	std::pair<int, float> outfour;
	std::pair<int, float> outfive;
	std::pair<int, float> outsix;

	const bool result1(q.try_pop(outone));
	const bool result2(q.try_pop(outtwo));
	const bool result3(q.try_pop(outthree));
	const bool result4(q.try_pop(outfour));
	const bool result5(q.try_pop(outfive));
	const bool result6(q.try_pop(outsix));
	const bool result7(q.try_pop(outsix));
	const bool result8(q.try_pop(outsix));
	const bool result9(q.try_pop(outsix));
	const bool result10(q.try_pop(outsix));

	//q.unsafe_reset_scratch_pool();

	std::pair<int, float> outFail;

	const bool failResult(q.try_pop(outFail));

	q.clear();

#if defined(GDUL_CPQ)
	using test_type = std::pair<int, float>;
#else
	using test_type = std::uint32_t;
#endif

	// At this point we need to test for correctness of values. Or. Perhaps multi-layer first.
	gdul::queue_testrun<test_type, gdul::tracking_allocator<std::pair<int, float>>>(
		1000000, 
		gdul::tracking_allocator<std::pair<int, float>>());


	gdul::concurrent_priority_queue<int, float, 512, gdul::cpq_allocation_strategy_scratch<std::allocator<std::uint8_t>>> test;
	test.push(one);
	test.try_pop(outone);
	test.unsafe_reset();
	test.unsafe_reset_scratch_pool();
	test.clear();
	test.empty();

	gdul::concurrent_priority_queue<int, float, 512, gdul::cpq_allocation_strategy_external> test2(1);
	decltype(test2)::node_type n;
	decltype(test2)::node_type* nOut(nullptr);
	n.m_kv = one;

	test2.push(&n);
	const bool resultN(test2.try_pop(nOut));

	test2.clear();
	const bool test2Empty(test2.empty());
	test2.unsafe_reset();
}

// Run program: Ctrl + F5 or Debug > Start Without Debugging menu
// Debug program: F5 or Debug > Start Debugging menu

// Tips for Getting Started: 
//   1. Use the Solution Explorer window to add/manage files
//   2. Use the Team Explorer window to connect to source control
//   3. Use the Output window to see build output and other messages
//   4. Use the Error List window to view errors
//   5. Go to Project > Add New Item to create new code files, or Project > Add Existing Item to add existing code files to the project
//   6. In the future, to open this project again, go to File > Open > Project and select the .sln file
