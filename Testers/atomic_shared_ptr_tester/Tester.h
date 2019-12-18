#pragma once

#include "..\Common\thread_pool.h"
#include "..\Common\util.h"
#include "..\Common\tracking_allocator.h"
#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>
#include <random>
#include <string>
#include "../Common/Timer.h"
#include <mutex>
#include <cassert>
#include <queue>

using namespace gdul;
#pragma warning(disable : 4324)
#pragma warning(disable:4200)
template <class T>
struct reference_comparison
{
	T* m_ptr;
	uint8_t trash[sizeof(atomic_shared_ptr<int>) - 8];
};

template <class T>
struct mutexed_wrapper
{
	std::shared_ptr<T> load(std::memory_order) {
		lock.lock();
		std::shared_ptr<T> returnValue(ptr);
		lock.unlock();
		return returnValue;
	}
	mutexed_wrapper() = default;
	mutexed_wrapper(std::shared_ptr<T>&& aPtr)
	{
		ptr.swap(aPtr);
	}
	mutexed_wrapper(mutexed_wrapper& aOther)
	{
		operator=(aOther.load());
	}
	mutexed_wrapper(mutexed_wrapper&& aOther)
	{
		operator=(std::move(aOther));
	}
	std::mutex lock;

	mutexed_wrapper& operator=(std::shared_ptr<T>& aPtr)
	{
		lock.lock();
		ptr = aPtr;
		lock.unlock();

		return *this;
	}
	mutexed_wrapper& operator=(std::shared_ptr<T>&& aPtr)
	{
		lock.lock();
		ptr.swap(aPtr);
		lock.unlock();
		return *this;
	}
	void Reset()
	{
		lock.lock();
		ptr.reset();
		lock.unlock();
	}
	bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T>&& desired, std::memory_order)
	{
		bool returnValue(false);
		lock.lock();
		if (ptr == expected){
			ptr = std::move(desired);
			returnValue = true;
		}
		else{
			expected = ptr;
		}
		lock.unlock();
		return returnValue;
	}
	std::shared_ptr<T> ptr;
};


template <class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
class tester
{
public:
	template <class ...InitArgs>
	tester(bool doInitArray = true, InitArgs && ...args);
	~tester();

	float execute(std::uint32_t passes, bool doAssign = true, bool doReassign = true, bool doCasTest = true, bool doRefTest = true, bool testAba = true);


	void work_assign(std::uint32_t passes);
	void work_reassign(std::uint32_t passes);
	void work_reference_test(std::uint32_t passes);
	void work_cas(std::uint32_t passes);
	void work_aba(std::uint32_t passes);

	void check_pointers() const;

	struct aba_node
	{
		aba_node() noexcept
			: m_next(nullptr)
			, m_owned(false)
		{
		}
		bool m_owned = false;
		atomic_shared_ptr<aba_node> m_next;
	};

	void aba_claim();
	void aba_release();

	static thread_local std::queue<shared_ptr<aba_node>> m_abaStorage;

	atomic_shared_ptr<aba_node> m_topAba;

	gdul::thread_pool m_worker;

#ifdef ASP_MUTEX_COMPARE
	mutexed_wrapper<T> m_testArray[ArraySize];
#else
	atomic_shared_ptr<T> m_testArray[ArraySize];
	reference_comparison<T> m_referenceComparison[ArraySize];
#endif
	std::atomic_bool m_workBlock;

	std::atomic<size_t> m_summary;

	std::random_device m_rd;
	std::mt19937 m_rng;
};
template <class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
thread_local std::queue<shared_ptr<typename tester<T, ArraySize, NumThreads>::aba_node>> tester<T, ArraySize, NumThreads>::m_abaStorage;

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
template<class ...InitArgs>
inline tester<T, ArraySize, NumThreads>::tester(bool doInitArray, InitArgs && ...args)
	: m_worker(NumThreads)
	, m_rng(m_rd())
#ifndef ASP_MUTEX_COMPARE
	, m_referenceComparison{ nullptr }
#endif
{
	if (doInitArray) {
		for (std::uint32_t i = 0; i < ArraySize; ++i) {
#ifndef ASP_MUTEX_COMPARE
			m_testArray[i] = make_shared<T>(std::forward<InitArgs&&>(args)...);
			m_referenceComparison[i].m_ptr = new T(std::forward<InitArgs&&>(args)...);
#else
			m_testArray[i] = std::make_shared<T>(std::forward<InitArgs&&>(args)...);
#endif
		}
	}
}

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline tester<T, ArraySize, NumThreads>::~tester()
{
#ifndef ASP_MUTEX_COMPARE
	for (std::uint32_t i = 0; i < ArraySize; ++i) {
		delete m_referenceComparison[i].m_ptr;
	}
#endif
}

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline float tester<T, ArraySize, NumThreads>::execute(std::uint32_t passes, bool doAssign, bool doReassign, bool doCasTest, bool doRefTest, bool testAba)
{
	m_workBlock.store(false);

	for (std::uint32_t thread = 0; thread < NumThreads; ++thread) {
		if (doAssign) {
			m_worker.add_task([this, passes]() { work_assign(passes); });
		}
		if (doReassign) {
			m_worker.add_task([this, passes]() { work_reassign(passes); });
		}
		if (doRefTest) {
			m_worker.add_task([this, passes]() { work_reference_test(passes); });
		}
		if (doCasTest) {
			m_worker.add_task([this, passes]() { work_cas(passes); });
		}
		if (testAba){
			m_worker.add_task([this, passes]() { work_aba(passes); });
		}
	}

	gdul::timer<float> time;

	m_workBlock.store(true);

	while (m_worker.has_unfinished_tasks()) {
		std::this_thread::yield();
	}

	check_pointers();

	std::cout << "Checksum" << m_summary << std::endl;

	return time.get();
}

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline void tester<T, ArraySize, NumThreads>::work_assign(std::uint32_t passes)
{
	while (!m_workBlock) {
		std::this_thread::yield();
	}

	for (std::uint32_t pass = 0; pass < passes; ++pass) {
		for (std::uint32_t i = 0; i < ArraySize; ++i) {
			m_testArray[i]
#ifdef ASP_MUTEX_COMPARE
				= std::make_shared<T>();
#else
				.store(make_shared<T>(), std::memory_order_relaxed);
#endif
		}
	}
}

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline void tester<T, ArraySize, NumThreads>::work_reassign(std::uint32_t passes)
{
	while (!m_workBlock) {
		std::this_thread::yield();
	}

	for (std::uint32_t pass = 0; pass < passes; ++pass) {
		for (std::uint32_t i = 0; i < ArraySize; ++i) {
#ifndef ASP_MUTEX_COMPARE
			m_testArray[i].store(m_testArray[(i + m_rng()) % ArraySize].load(std::memory_order_relaxed), std::memory_order_relaxed);
#else
			m_testArray[i] = m_testArray[(i + m_rng()) % ArraySize].load(std::memory_order::memory_order_acquire);
#endif
		}
	}

}
template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline void tester<T, ArraySize, NumThreads>::work_reference_test(std::uint32_t passes)
{
#ifndef ASP_MUTEX_COMPARE
	while (!m_workBlock) {
		std::this_thread::yield();
	}

	size_t localSum(0);

	for (std::uint32_t pass = 0; pass < passes; ++pass) {
		for (std::uint32_t i = 0; i < ArraySize; ++i) {
			localSum += *m_referenceComparison[i].m_ptr;
			//localSum += *m_testArray[i];
		}
	}
	m_summary += localSum;
#else
	passes;
#endif
}

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline void tester<T, ArraySize, NumThreads>::work_cas(std::uint32_t passes)
{

	while (!m_workBlock) {
		std::this_thread::yield();
	}

	size_t localSum(0);

	for (std::uint32_t pass = 0; pass < passes; ++pass) {
		for (std::uint32_t i = 0; i < ArraySize; ++i) {

#ifndef ASP_MUTEX_COMPARE
			shared_ptr<T> desired(make_shared<T>());
			shared_ptr<T> expected(m_testArray[i].load(std::memory_order_relaxed));
			raw_ptr<T> check(expected);
			const bool resulta = m_testArray[i].compare_exchange_strong(expected, std::move(desired), std::memory_order_relaxed);

			shared_ptr<T> desired_(make_shared<T>());
			shared_ptr<T> expected_(m_testArray[i].load(std::memory_order_relaxed));
			raw_ptr<T> rawExpected(expected_);
			raw_ptr<T> check_(expected_);
			const bool resultb = m_testArray[i].compare_exchange_strong(rawExpected, std::move(desired_), std::memory_order_relaxed);

			if (!(resultb == (rawExpected == check_))) {
				throw std::runtime_error("output from expected do not correspond to CAS results");
			}

#else
			std::shared_ptr<T> desired_(std::make_shared<T>());
			std::shared_ptr<T> expected_(m_testArray[i].load(std::memory_order_acquire));
			const bool resultb = m_testArray[i].compare_exchange_strong(expected_, std::move(desired_), std::memory_order_acquire);
#endif
		}
	}

	m_summary += localSum;
}

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline void tester<T, ArraySize, NumThreads>::work_aba(std::uint32_t passes)
{
	for (std::uint32_t i = 0; i < passes; ++i)
	{
		aba_claim();
		aba_release();
	}
}
template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline void tester<T, ArraySize, NumThreads>::aba_claim()
{
	shared_ptr<aba_node> top(m_topAba.load(std::memory_order_relaxed));
	while (top)
	{
		shared_ptr<aba_node> next(top->m_next.load());

		if (m_topAba.compare_exchange_strong(top, next, std::memory_order_seq_cst, std::memory_order_relaxed))
		{
			GDUL_ASSERT(!top->m_owned);
			top->m_owned = true;
			m_abaStorage.push(std::move(top));

			return;
		}
	}
	m_abaStorage.push(make_shared<aba_node, tracking_allocator<aba_node>>());
}

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline void tester<T, ArraySize, NumThreads>::aba_release()
{
	shared_ptr<aba_node> toRelease(nullptr);

	toRelease = m_abaStorage.front();
	m_abaStorage.pop();
	toRelease->m_owned = false;

	shared_ptr<aba_node> top(m_topAba.load());
	do
	{
		toRelease->m_next.store(top);
	} while (!m_topAba.compare_exchange_strong(top, std::move(toRelease)));
}
template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline void tester<T, ArraySize, NumThreads>::check_pointers() const
{
#ifndef ASP_MUTEX_COMPARE
	//uint32_t count(0);
	//for (std::uint32_t i = 0; i < ArraySize; ++i) {
	//	const gdul::aspdetail::control_block_base<T, gdul::aspdetail::default_allocator>* const controlBlock(m_testArray[i].unsafe_get_control_block());
	//	const T* const directObject(m_testArray[i].unsafe_get());
	//	const T* const sharedObject(controlBlock->get());
	//
	//	if (directObject != sharedObject) {
	//		++count;
	//	}
	//}
	//std::cout << "Mismatch shared / object count: " << count << std::endl;
#endif
}
