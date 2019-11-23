

#include "../Common/thread_pool.h"
#include "../Common/util.h"
#include "../Common/tracking_allocator.h"
#include <gdul/concurrent_queue/concurrent_queue.h>
#include <gdul/WIP_thread_local_member/thread_local_member.h>

namespace gdul
{

template <class T>
class tester
{
public:
	tester(const T& init);
	void execute();

	void test_index_pool(uint32_t tasks);
	void test_construction(uint32_t tasks);
	void test_assignment(uint32_t tasks);

	gdul::thread_pool m_worker;

	gdul::tracking_allocator<T> m_alloc;

	using alloc_t = decltype(m_alloc);
	using tlm_t = gdul::tlm<T, alloc_t>;

	gdul::tlm_detail::index_pool<> m_indexPool;

	const T m_init;
};

template <class T>
tester<T>::tester(const T& init)
	: m_init(init)
	, m_worker()
{
}
template <class T>
void tester<T>::execute()
{
	test_assignment(100);
	test_construction(100);
	test_index_pool(100);

	while (m_worker.has_unfinished_tasks()) {
		std::this_thread::yield();
	}

	gdul::tlm<T, alloc_t>::_unsafe_reset();
	
	std::cout << "final: " << gdul::s_allocated << std::endl;
}
template<class T>
inline void tester<T>::test_index_pool(uint32_t tasks)
{
	auto lam = [this]
	{
		auto one = m_indexPool.get(m_alloc);
		auto two = m_indexPool.get(m_alloc);
		auto three = m_indexPool.get(m_alloc);
		auto four = m_indexPool.get(m_alloc);
		auto five = m_indexPool.get(m_alloc);

		m_indexPool.add(five);
		m_indexPool.add(two);
		m_indexPool.add(one);
		m_indexPool.add(four);
		m_indexPool.add(three);
	};
	for (uint32_t i = 0; i < tasks; ++i)
	{
		m_worker.add_task(lam);
	}
}
template<class T>
inline void tester<T>::test_construction(uint32_t tasks)
{
	auto lam = [this]()
	{
		for (int i = 0; i < 40; ++i)
		{
			gdul::tlm<T, alloc_t> first(m_init);
			gdul::tlm<T, alloc_t> second(m_init);
			gdul::tlm<T, alloc_t> third(m_init);
			gdul::tlm<T, alloc_t> fourth(m_init);
			gdul::tlm<T, alloc_t> fifth(m_init);

			GDUL_ASSERT(first == m_init);
			GDUL_ASSERT(second == m_init);
			GDUL_ASSERT(third == m_init);
			GDUL_ASSERT(fourth == m_init);
			GDUL_ASSERT(fifth == m_init);
		}
	};
	for (uint32_t i = 0; i < tasks; ++i)
	{
		m_worker.add_task(lam);
	}
}
template<class T>
inline void tester<T>::test_assignment(uint32_t tasks)
{
	auto lam = [this]()
	{
		gdul::tlm<T, alloc_t> first;
		gdul::tlm<T, alloc_t> second;
		gdul::tlm<T, alloc_t> third;
		gdul::tlm<T, alloc_t> fourth;
		gdul::tlm<T, alloc_t> fifth;

		const T firstExp = T();
		const T secondExp = T();
		const T thirdExp = T();
		const T fourthExp = T();
		const T fifthExp = T();

		first = firstExp;
		second = secondExp;
		third = thirdExp;
		fourth = fourthExp;
		fifth = fifthExp;

		const T firstOut = first;
		const T secondOut = second;
		const T thirdOut = third;
		const T fourthOut = fourth;
		const T fifthOut = fifth;


		GDUL_ASSERT(firstOut == firstExp);
		GDUL_ASSERT(secondOut == secondExp);
		GDUL_ASSERT(thirdOut == thirdExp);
		GDUL_ASSERT(fourthOut == fourthExp);
		GDUL_ASSERT(fifthOut == fifthExp);

	};
	for (uint32_t i = 0; i < tasks; ++i)
	{
		m_worker.add_task(lam);
	}
}
}
