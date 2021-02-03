// ConcurrentObjectPool.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include "pch.h"
#include <iostream>
#include <gdul/memory/concurrent_object_pool.h>
#include <gdul/memory/concurrent_guard_pool.h>
#include <gdul/memory/pool_allocator.h>
#include <gdul/WIP/scratch_allocator.h>
#include <gdul/WIP/small_vector.h>

#include <vld.h>

void test_small_vector()
{
	gdul::scratch_pad<64> pad;
	gdul::scratch_allocator<int> sall(pad.create_allocator<int>());

	int* a = sall.allocate(10);
	int* b = sall.allocate(5);
	sall.deallocate(a, 10);
	sall.deallocate(b, 5);

	std::array<int, 5> collection{ 1, 2, 3, 4, 5 };

	gdul::small_vector<int> c1;
	gdul::small_vector<int> c2 = gdul::small_vector<int>(std::allocator<int>());
	gdul::small_vector<int> c3(5, 5);
	gdul::small_vector<int> c4 = gdul::small_vector<int>(5, std::allocator<int>());
	gdul::small_vector<int> c5(collection.begin(), collection.end());
	gdul::small_vector<int> c6(c5);
	gdul::small_vector<int> c7(c5, std::allocator<int>());
	gdul::small_vector<int> c8(std::move(c5));
	gdul::small_vector<int> c9(std::move(c6), std::allocator<int>());
	gdul::small_vector<int> c10({ 1, 2, 3, 4, 5 });
	std::vector<int> vec({ 1, 2, 3, 4, 5 });
	gdul::small_vector<int> c11(vec);
	gdul::small_vector<int> c12(std::vector<int>({ 1, 2, 3, 4, 5 }));

	gdul::small_vector<int> assign;
	assign = c10;
	gdul::small_vector<int> move;
	move = std::move(assign);

	gdul::small_vector<int> swap;
	swap.swap(move);

	std::vector<int> convert(swap);

	std::allocator<int> alloc(c1.get_allocator());
}


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

	gdul::memory_pool mempool;
	mempool.init<sizeof(int), alignof(int)>(4, 10);
	gdul::pool_allocator<int> mpAlloc(mempool.create_allocator<int>());


	int* items1 = mpAlloc.allocate();
	int* items2 = mpAlloc.allocate();
	int* items3 = mpAlloc.allocate();
	int* items4 = mpAlloc.allocate();

	mpAlloc.deallocate(items1);
	mpAlloc.deallocate(items2);
	mpAlloc.deallocate(items3);
	mpAlloc.deallocate(items4);

	constexpr gdul::least_unsigned_integer_t<5> ch(5);
	constexpr gdul::least_unsigned_integer_t<257> sh(5);
	constexpr gdul::least_unsigned_integer_t<std::numeric_limits<int>::max()> ui(5);
	constexpr gdul::least_unsigned_integer_t<std::numeric_limits<long long int>::max()> uli(5);

	test_small_vector();

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
