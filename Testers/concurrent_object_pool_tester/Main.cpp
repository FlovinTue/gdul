// ConcurrentObjectPool.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <gdul\concurrent_object_pool\concurrent_object_pool.h>
#include <vld.h>

struct test
{
	~test()
	{
		std::cout << "destructed" << std::endl;
	}
	std::shared_ptr<test> next;
};

int main()
{
	std::shared_ptr<test> next(std::make_shared<test>());
	next->next = std::make_shared<test>();
	next = std::move(next->next);

	//std::allocator<std::uint8_t> alloc;
	//gdul::concurrent_object_pool<int, decltype(alloc)> pool(1, alloc);
	//int* second = pool.get_object();
	//int* first = pool.get_object();
	//pool.recycle_object(first);
	//pool.unsafe_destroy();
	//std::size_t numAvaliable = pool.avaliable();

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
