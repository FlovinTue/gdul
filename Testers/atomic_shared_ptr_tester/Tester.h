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

#define CXX20_ATOMIC_SHARED_PTR


#pragma warning(disable : 4324)
#pragma warning(disable:4200)
template <class T>
struct reference_comparison
{
	T* m_ptr;
};

template <class T>
struct mutexed_wrapper
{
	std::shared_ptr<T> load(std::memory_order) {
#if defined(CXX20_ATOMIC_SHARED_PTR)
		return ptr.load();
#else
		lock.lock();
		std::shared_ptr<T> returnValue(ptr);
		lock.unlock();
		return returnValue;
#endif
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
#if !defined(CXX20_ATOMIC_SHARED_PTR)
	std::mutex lock;
#endif

	mutexed_wrapper& operator=(std::shared_ptr<T>& aPtr)
	{
#if defined(CXX20_ATOMIC_SHARED_PTR)
		ptr.store(aPtr);
#else
		lock.lock();
		ptr = aPtr;
		lock.unlock();
#endif
		return *this;
	}
	mutexed_wrapper& operator=(std::shared_ptr<T>&& aPtr)
	{
#if defined(CXX20_ATOMIC_SHARED_PTR)
		ptr.store(std::move(aPtr));
#else
		lock.lock();
		ptr.swap(aPtr);
		lock.unlock();
#endif
		return *this;
	}
	void Reset()
	{
#if defined(CXX20_ATOMIC_SHARED_PTR)
		ptr.reset();
#else
		lock.lock();
		ptr.reset();
		lock.unlock();
#endif
	}
	bool compare_exchange_strong(std::shared_ptr<T>& expected, std::shared_ptr<T>&& desired, std::memory_order)
	{
#if defined(CXX20_ATOMIC_SHARED_PTR)
		return ptr.compare_exchange_strong(expected, std::move(desired), std::memory_order_seq_cst);
#else
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
#endif
	}
#if defined(CXX20_ATOMIC_SHARED_PTR)
	std::atomic<std::shared_ptr<T>> ptr;
#else
	std::shared_ptr<T> ptr;
#endif
};


template <class T, std::uint32_t ArraySize>
class asp_tester
{
public:
	template <class ...InitArgs>
	asp_tester(bool doInitArray = true, InitArgs && ...args);
	~asp_tester();

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
		gdul::atomic_shared_ptr<aba_node> m_next;
	};

	void aba_claim();
	void aba_release();

	static thread_local std::queue<gdul::shared_ptr<aba_node>> m_abaStorage;

	gdul::atomic_shared_ptr<aba_node> m_topAba;

	gdul::thread_pool m_worker;

#ifdef ASP_MUTEX_COMPARE
	mutexed_wrapper<T> m_testArray[ArraySize];
#else
	gdul::atomic_shared_ptr<T> m_testArray[ArraySize];
	reference_comparison<T> m_referenceComparison[ArraySize]{};
#endif
	std::atomic_bool m_workBlock;

	std::atomic<size_t> m_summary;

	std::random_device m_rd;
	std::mt19937 m_rng;
};
template <class T, std::uint32_t ArraySize>
thread_local std::queue<gdul::shared_ptr<typename asp_tester<T, ArraySize>::aba_node>> asp_tester<T, ArraySize>::m_abaStorage;

template<class T, std::uint32_t ArraySize>
template<class ...InitArgs>
inline asp_tester<T, ArraySize>::asp_tester(bool doInitArray, InitArgs && ...args)
	: m_worker(std::numeric_limits<std::uint8_t>::max())
	, m_rng(m_rd())
{
	if (doInitArray) {
		for (std::uint32_t i = 0; i < ArraySize; ++i) {
#ifndef ASP_MUTEX_COMPARE
			m_testArray[i] = gdul::make_shared<T>(std::forward<InitArgs&&>(args)...);
			m_referenceComparison[i].m_ptr = new T(std::forward<InitArgs&&>(args)...);
#else
			m_testArray[i] = std::make_shared<T>(std::forward<InitArgs&&>(args)...);
#endif
		}
	}
}

template<class T, std::uint32_t ArraySize>
inline asp_tester<T, ArraySize>::~asp_tester()
{
#ifndef ASP_MUTEX_COMPARE
	for (std::uint32_t i = 0; i < ArraySize; ++i) {
		delete m_referenceComparison[i].m_ptr;
	}
#endif
}

template<class T, std::uint32_t ArraySize>
inline float asp_tester<T, ArraySize>::execute(std::uint32_t passes, bool doAssign, bool doReassign, bool doCasTest, bool doRefTest, bool testAba)
{
	m_workBlock.store(false);

	for (std::uint32_t thread = 0; thread < m_worker.worker_count(); ++thread) {
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

template<class T, std::uint32_t ArraySize>
inline void asp_tester<T, ArraySize>::work_assign(std::uint32_t passes)
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
				.store(gdul::make_shared<T>(), std::memory_order_relaxed);
#endif
		}
	}
}

template<class T, std::uint32_t ArraySize>
inline void asp_tester<T, ArraySize>::work_reassign(std::uint32_t passes)
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
template<class T, std::uint32_t ArraySize>
inline void asp_tester<T, ArraySize>::work_reference_test(std::uint32_t passes)
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

template<class T, std::uint32_t ArraySize>
inline void asp_tester<T, ArraySize>::work_cas(std::uint32_t passes)
{

	while (!m_workBlock) {
		std::this_thread::yield();
	}

	size_t localSum(0);

	for (std::uint32_t pass = 0; pass < passes; ++pass) {
		for (std::uint32_t i = 0; i < ArraySize; ++i) {

#ifndef ASP_MUTEX_COMPARE
			gdul::shared_ptr<T> desired(gdul::make_shared<T>());
			gdul::shared_ptr<T> expected(m_testArray[i].load(std::memory_order_relaxed));
			gdul::raw_ptr<T> check(expected);
			const bool resulta = m_testArray[i].compare_exchange_strong(expected, std::move(desired), std::memory_order_relaxed);

			//gdul::shared_ptr<T> desired_(gdul::make_shared<T>());
			//gdul::shared_ptr<T> expected_(m_testArray[i].load(std::memory_order_relaxed));
			//gdul::raw_ptr<T> rawExpected(expected_);
			//gdul::raw_ptr<T> check_(expected_);
			//const bool resultb = m_testArray[i].compare_exchange_strong(rawExpected, std::move(desired_), std::memory_order_relaxed);

			//if (!(resultb == (rawExpected == check_ && rawExpected.get_version() == check_.get_version()))) {
			//	throw std::runtime_error("output from expected do not correspond to CAS results");
			//}

#else
			std::shared_ptr<T> desired_(std::make_shared<T>());
			std::shared_ptr<T> expected_(m_testArray[i].load(std::memory_order_acquire));
			const bool resultb = m_testArray[i].compare_exchange_strong(expected_, std::move(desired_), std::memory_order_acquire);
#endif
		}
	}

	m_summary += localSum;
}

template<class T, std::uint32_t ArraySize>
inline void asp_tester<T, ArraySize>::work_aba(std::uint32_t passes)
{
	for (std::uint32_t i = 0; i < passes; ++i)
	{
		aba_claim();
		aba_release();
	}
}
template<class T, std::uint32_t ArraySize>
inline void asp_tester<T, ArraySize>::aba_claim()
{
	gdul::shared_ptr<aba_node> top(m_topAba.load(std::memory_order_relaxed));
	while (top)
	{
		gdul::shared_ptr<aba_node> next(top->m_next.load());

		if (m_topAba.compare_exchange_strong(top, next, std::memory_order_seq_cst, std::memory_order_relaxed))
		{
			GDUL_ASSERT(!top->m_owned);
			top->m_owned = true;
			m_abaStorage.push(std::move(top));

			return;
		}
	}
	m_abaStorage.push(gdul::allocate_shared<aba_node>(gdul::tracking_allocator<aba_node>()));
}

template<class T, std::uint32_t ArraySize>
inline void asp_tester<T, ArraySize>::aba_release()
{
	gdul::shared_ptr<aba_node> toRelease(nullptr);

	toRelease = m_abaStorage.front();
	m_abaStorage.pop();
	toRelease->m_owned = false;

	gdul::shared_ptr<aba_node> top(m_topAba.load());
	do
	{
		toRelease->m_next.store(top);
	} while (!m_topAba.compare_exchange_strong(top, std::move(toRelease)));
}
template<class T, std::uint32_t ArraySize>
inline void asp_tester<T, ArraySize>::check_pointers() const
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
