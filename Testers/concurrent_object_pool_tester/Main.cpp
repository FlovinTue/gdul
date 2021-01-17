// ConcurrentObjectPool.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <gdul/memory/concurrent_object_pool.h>
#include <gdul/memory/concurrent_guard_pool.h>
#include <vld.h>
#include <vector>

float func(int arg)
{
	return (float)arg;
}

int main()
{
	gdul::concurrent_guard_pool<int> gp;

	std::allocator<std::uint8_t> alloc;
	gdul::concurrent_object_pool<int, decltype(alloc)> pool(alloc);

	int* second = pool.get();
	second;
	int* first = pool.get();
	pool.recycle(first);
	pool.unsafe_reset();
	std::size_t numAvaliable = pool.avaliable();
	numAvaliable;

	std::vector<int*> out;
	for (auto i = 0; i < 4; ++i)
	{
		auto elem(pool.get());
		*elem = i;
		out.push_back(elem);
	}

	for (auto& elem : out)
		pool.recycle(elem);

	int* swapA(pool.get());

	pool.recycle(swapA);

	gdul::atomic_shared_ptr<int> yo;

    std::cout << "Hello World!\n"; 
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
