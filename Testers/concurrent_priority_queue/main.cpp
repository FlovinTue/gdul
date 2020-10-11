// concurrent_priority_queue.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <gdul/WIP_concurrent_priority_queue/concurrent_priority_queue_v9.h>

#define GDUL_CPQ
//#define MS_CPQ
#include "../queue_tester/tester.h"

namespace gdul {
std::random_device rd;
std::mt19937 rng(rd());
}

gdul::concurrent_priority_queue<int, float> q;

using node_t = decltype(q)::node_type;

#if defined(GDUL_CPQ)
thread_local gdul::shared_ptr<node_t[]> gdul::tester< std::pair<int, float>, gdul::tracking_allocator<std::pair<int, float>>>::m_nodes;
thread_local std::vector< node_t*> gdul::tester< std::pair<int, float>, gdul::tracking_allocator<std::pair<int, float>>>::t_output;
#endif
int main()
{

	node_t two(std::make_pair	(2, 1.f));
	node_t six(std::make_pair	(6, 1.f));
	node_t three(std::make_pair	(3, 1.f));
	node_t four(std::make_pair	(4, 1.f));
	node_t one(std::make_pair	(1, 1.f));
	node_t five(std::make_pair	(5, 1.f));

	q.push(&two);
	q.push(&six);
	q.push(&three);
	q.push(&four);
	q.push(&one);
	q.push(&five);
	
	node_t* outone(nullptr);
	node_t* outtwo(nullptr);
	node_t* outthree(nullptr);
	node_t* outfour(nullptr);
	node_t* outfive(nullptr);
	node_t* outsix(nullptr);

	const bool result1(q.try_pop(outone));
	const bool result2(q.try_pop(outtwo));
	const bool result3(q.try_pop(outthree));
	const bool result4(q.try_pop(outfour));
	const bool result5(q.try_pop(outfive));
	const bool result6(q.try_pop(outsix));

	node_t* outFail(nullptr);

	const bool failResult(q.try_pop(outFail));

	q.clear();

#if defined(GDUL_CPQ)
	using test_type = std::pair<int, float>;
#else
	using test_type = std::uint32_t;
#endif

	// At this point we need to test for correctness of values. Or. Perhaps multi-layer first.
	gdul::queue_testrun<test_type, gdul::tracking_allocator<test_type>>(
		10000, 
		gdul::tracking_allocator<std::pair<int, float>>(), 
		/*gdul::test_option_single |  gdul::test_option_onlyRead|gdul::test_option_onlyWrite  | */gdul::test_option_singleReadWrite);

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
