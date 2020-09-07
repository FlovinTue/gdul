// concurrent_priority_queue.cpp : This file contains the 'main' function. Program execution begins and ends there.
//
#include <gdul/WIP_concurrent_priority_queue/concurrent_priority_queue_v7.h>

int main()
{
	gdul::concurrent_priority_queue<int, float> q;

	using node_t = decltype(q)::node_type;

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
