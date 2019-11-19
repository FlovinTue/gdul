

#include "../Common/thread_pool.h"
#include "../Common/tracking_allocator.h"
#include <gdul/concurrent_queue/concurrent_queue.h>
#include <gdul/WIP_thread_local_member/thread_local_member.h>

namespace gdul
{

template <class Dummy = void>
class tester
{
public:

	void execute();

	gdul::thread_pool m_worker;

	gdul::tracking_allocator<uint8_t> m_alloc;

	using alloc_t = decltype(m_alloc);
};

template <class Dummy>
void tester<Dummy>::execute()
{
	{
		//auto lam = []()
		//{
		//	gdul::tracking_allocator<int> alloc;
		//	using all = decltype(alloc);
		//	{
		//		for (int i = 0; i < 40; ++i)
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

		gdul::tlm_detail::index_pool<gdul::tracking_allocator<int>> ind;
		auto lam = [&ind] {
			gdul::tracking_allocator<int> alloc;

			{
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
			}
			{
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
			}
			{
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
			}
		};

		gdul::concurrent_queue<std::function<void()>> que;

		for (uint32_t i = 0; i < 180000; ++i)
		{
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
		gdul::tlm<int, gdul::tracking_allocator<int>>::_unsafe_reset();
		gdul::tlm<std::string, gdul::tracking_allocator<int>>::_unsafe_reset();
	}
	std::cout << "final: " << gdul::s_allocated << std::endl;
}
}
