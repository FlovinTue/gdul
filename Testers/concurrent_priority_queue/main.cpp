// concurrent_priority_queue.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <gdul/WIP_concurrent_priority_queue/op_test.h>
#include <concurrent_priority_queue.h>


int main()
{
	gdul::cpq q;
	

	q.push(9);
	q.push(8);
	q.push(7);
	q.push(6);
	q.push(5);
	q.push(4);
	q.push(3);
	q.push(2);
	q.push(1);

	std::vector<std::uint32_t> outItems;

	for (std::size_t i = 0; i < 9; ++i) {
		uint32_t out(0);
		const bool result(q.try_pop(out));
		assert(result && "Expected success");

		outItems.push_back(out);
	}

	uint32_t testOut(0);
	const bool result(q.try_pop(testOut));
	assert(!result && "Expected failure");

	concurrency::concurrent_priority_queue<int> blah;

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
