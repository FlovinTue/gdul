

#include "../Common/thread_pool.h"
#include "../Common/util.h"
#include "../Common/tracking_allocator.h"
#include <gdul/concurrent_queue/concurrent_queue.h>
#include <gdul/thread_local_member/thread_local_member.h>

namespace gdul
{

template <class T>
class tester
{
public:
	tester(const T& init);
	void execute();

	void assert_base_functionality();

	void test_index_pool(uint32_t tasks);
	void test_construction(uint32_t tasks);
	void test_assignment(uint32_t tasks);

	gdul::thread_pool m_worker;

	gdul::tracking_allocator<T> m_alloc;

	using alloc_t = decltype(m_alloc);
	using tlm_t = gdul::tlm<T, alloc_t>;

	gdul::tlm_detail::index_pool<void> m_indexPool;

	const T m_init;
};

template <class T>
tester<T>::tester(const T& init)
	: m_init(init)
	, m_worker(std::numeric_limits<std::uint8_t>::max())
{
	assert_base_functionality();
}
template <class T>
void tester<T>::execute()
{
	test_assignment(10000);
	test_construction(10000);
	test_index_pool(10000);

	while (m_worker.has_unfinished_tasks()) {
		std::this_thread::yield();
	}
	
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
		gdul::tlm<T, alloc_t> first(0);
		gdul::tlm<T, alloc_t> second(1);
		gdul::tlm<T, alloc_t> third(2);
		gdul::tlm<T, alloc_t> fourth(3);
		gdul::tlm<T, alloc_t> fifth(4);

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
template<class T>
inline void tester<T>::assert_base_functionality()
{
	tlm<int> resetTest(5);
	const int five(resetTest.get());
	resetTest.unsafe_reset(6);
	const int six(resetTest.get());
	GDUL_ASSERT(six == 6);

	tlm<char> boolOpFalse(0);
	tlm<char> boolOpTrue(1);

	GDUL_ASSERT(!(bool)boolOpFalse);
	GDUL_ASSERT((bool)boolOpTrue);

	tlm<int> implicit(5);
	int& accessImpl(implicit);
	GDUL_ASSERT(accessImpl == 5);

	const tlm<int> implicitConst(6);
	const int& accessImplConst(implicitConst);
	GDUL_ASSERT(accessImplConst == 6);

	const tlm<int> equalA(8);
	const int equalB(8);
	GDUL_ASSERT(equalA == equalB);
	GDUL_ASSERT(equalB == equalA);

	const tlm<int> nequalA(9);
	const int nequalB(10);
	GDUL_ASSERT(nequalA != nequalB);
	GDUL_ASSERT(nequalB != nequalA);

	tlm<int> assign(0);
	assign = 5;

	GDUL_ASSERT(assign.get() == 5);

	struct src
	{
		int mem = 0;
	}sr;

	tlm<src*> ref(&sr);
	const tlm<src*> cref(&sr);
	ref->mem = 5;

	GDUL_ASSERT(cref->mem == 5);
	
	src out = *ref;
	const src out2 = *cref;

	GDUL_ASSERT(out.mem == 5);
	GDUL_ASSERT(out2.mem == 5);
}
}
