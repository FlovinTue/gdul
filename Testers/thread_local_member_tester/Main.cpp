
#include "pch.h"
#include <iostream>
#include <gdul\WIP_thread_local_member\thread_local_member.h>
#include <thread>
#include <vld.h>
#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>
#include <gdul\concurrent_queue\concurrent_queue.h>
#include <functional>
#include <vector>

namespace gdul {
static std::atomic_flag ourSpin{ ATOMIC_FLAG_INIT };
static std::atomic<int64_t> ourAllocated(0);
template <class T>
class tracking_allocator : public std::allocator<T>
{
public:
	tracking_allocator() = default;

	template <typename U>
	struct rebind
	{
		using other = tracking_allocator<U>;
	};
	tracking_allocator(const tracking_allocator<T>&) = default;
	tracking_allocator& operator=(const tracking_allocator<T>&) = default;

	T* allocate(std::size_t count)
	{
		ourAllocated += count * sizeof(T);
		T* ret = std::allocator<T>::allocate(count);
		return ret;
	}
	void deallocate(T* obj, std::size_t count)
	{
		ourAllocated -= count * sizeof(T);
		std::allocator<T>::deallocate(obj, count);
	}
	void spin()
	{
		while (ourSpin.test_and_set()) {
			std::this_thread::sleep_for(std::chrono::microseconds(10));
		}
	}
	void release()
	{
		ourSpin.clear();
	}
	template <class Other>
	tracking_allocator(const tracking_allocator<Other>&) {};
	template <class Other>
	tracking_allocator<Other>& operator=(const tracking_allocator<Other>&) {};

};
}

int main()
{
	//auto lam = []()
	//{
	//	gdul::tracking_allocator<int> alloc;
	//	using all = decltype(alloc);
	//	{
	//		for (int i = 0; i < 10; ++i)
	//		{
	//			gdul::tlm<int, std::allocator<int>> aliased(i);
	//			aliased = 5;
	//		}
	//		gdul::tlm<int, all> aliased(555);
	//		gdul::tlm<int, all> aliased1(222);
	//		gdul::tlm<int, all> aliased2(333);
	//		gdul::tlm<int, all> aliased3(111);
	//		gdul::tlm<int, all> aliased4(777);
	//		gdul::tlm<int, all> aliased5(888);
	//		gdul::tlm<int, all> aliased6(999);

	//		const int heja = aliased;
	//		aliased = 1;
	//	}
	//	{
	//		gdul::thread_local_member<std::string, gdul::tracking_allocator<int>> first;
	//		gdul::thread_local_member<std::string, gdul::tracking_allocator<int>> second;
	//		gdul::thread_local_member<std::string, gdul::tracking_allocator<int>> third;
	//		gdul::thread_local_member<std::string, gdul::tracking_allocator<int>> fourth;
	//		gdul::thread_local_member<std::string, gdul::tracking_allocator<int>> fifth;
	//		first = "first john long long john brother tuck";
	//		second = "second john long long john brother tuck";
	//		third = "third john long long john brother tuck";
	//		fourth = "fourth john long long john brother tuck";
	//		fifth = "fifth john long long john brother tuck";

	//		std::string firstOut = first;
	//		std::string secondOut = second;
	//		std::string thirdOut = third;
	//		std::string fourthOut = fourth;
	//		std::string fifthOut = fifth;
	//	}
	//	{
	//		gdul::thread_local_member<int, gdul::tracking_allocator<int>> first;
	//		gdul::thread_local_member<int, gdul::tracking_allocator<int>> second;
	//		gdul::thread_local_member<int, gdul::tracking_allocator<int>> third;
	//		gdul::thread_local_member<int, gdul::tracking_allocator<int>> fourth;
	//		gdul::thread_local_member<int, gdul::tracking_allocator<int>> fifth;

	//		first = 1;
	//		second = 2;
	//		third = 3;
	//		fourth = 4;
	//		fifth = 5;

	//		const int firstOut = first;
	//		const int secondOut = second;
	//		const int thirdOut = third;
	//		const int fourthOut = fourth;
	//		const int fifthOut = fifth;
	//	}
	//};

	{
		gdul::tlm_detail::index_pool<gdul::tracking_allocator<int>> ind;
		auto lam = [&ind] {
			gdul::tracking_allocator<int> alloc;

			auto one = ind.get(alloc);
			auto two = ind.get(alloc);
			auto three = ind.get(alloc);
			auto four = ind.get(alloc);
			auto five = ind.get(alloc);

			ind.add(five);
			ind.add(two);
			ind.add(one);
			ind.add(four);
			ind.add(three);
		};

		gdul::concurrent_queue<std::function<void()>> que;

		for (uint32_t i = 0; i < 20000; ++i) {
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

	}
	std::cout << "final: " << gdul::ourAllocated << std::endl;

	return 0;
}