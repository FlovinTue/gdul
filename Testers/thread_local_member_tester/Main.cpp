
#include "pch.h"
#include <iostream>
#include <gdul\WIP_thread_local_member\thread_local_member.h>
#include <thread>
#include <vld.h>
#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>
#include <gdul\concurrent_queue\concurrent_queue.h>
#include <functional>
#include <vector>

int main()
{
	auto lam = []()
	{
		std::allocator<int> alloc;
		{
			for (int i = 0; i < 10; ++i)
			{
				gdul::tlm<int, std::allocator<int>> aliased(i);
				aliased = 5;
			}
			gdul::tlm<int, std::allocator<int>> aliased(555);
			gdul::tlm<int, std::allocator<int>> aliased1(222);
			gdul::tlm<int, std::allocator<int>> aliased2(333);
			gdul::tlm<int, std::allocator<int>> aliased3(111);
			gdul::tlm<int, std::allocator<int>> aliased4(777);
			gdul::tlm<int, std::allocator<int>> aliased5(888);
			gdul::tlm<int, std::allocator<int>> aliased6(999);

			const int heja = aliased;
			aliased = 1;
		}
		{
			gdul::thread_local_member<std::string> first;
			gdul::thread_local_member<std::string> second;
			gdul::thread_local_member<std::string> third;
			gdul::thread_local_member<std::string> fourth;
			gdul::thread_local_member<std::string> fifth;
			first = "first john long long john brother tuck";
			second = "second john long long john brother tuck";
			third = "third john long long john brother tuck";
			fourth = "fourth john long long john brother tuck";
			fifth = "fifth john long long john brother tuck";

			std::string firstOut = first;
			std::string secondOut = second;
			std::string thirdOut = third;
			std::string fourthOut = fourth;
			std::string fifthOut = fifth;
		}
		{
			gdul::thread_local_member<int> first;
			gdul::thread_local_member<int> second;
			gdul::thread_local_member<int> third;
			gdul::thread_local_member<int> fourth;
			gdul::thread_local_member<int> fifth;

			first = 1;
			second = 2;
			third = 3;
			fourth = 4;
			fifth = 5;

			const int firstOut = first;
			const int secondOut = second;
			const int thirdOut = third;
			const int fourthOut = fourth;
			const int fifthOut = fifth;
		}
	};

	gdul::concurrent_queue<std::function<void()>> que;

	for (uint32_t i = 0; i < 20000; ++i){
		que.push(lam);
	}

	auto consume = [&que]()
	{
		std::function<void()> out;

		while (que.try_pop(out))
		{
			out();
		}
	};

	std::vector<std::thread> threads;

	for (uint32_t i = 0; i < 8; ++i)
	{
		threads.push_back(std::thread(consume));
	}

	for (uint32_t i = 0; i < 8; ++i)
	{
		threads[i].join();
	}

	return 0;
}