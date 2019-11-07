#pragma once

#include "ThreadPool.h"
#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>
#include <random>
#include <string>
#include "Timer.h"
#include <mutex>
#include <cassert>

using namespace gdul;
#pragma warning(disable : 4324)
#pragma warning(disable:4200)
template <class T>
struct ReferenceComparison
{
	T* m_ptr;
	uint8_t trash[sizeof(atomic_shared_ptr<int>) - 8];
};

template <class T>
struct MutextedWrapper
{
	std::shared_ptr<T> load(std::memory_order) {
		lock.lock();
		std::shared_ptr<T> returnValue(ptr);
		lock.unlock();
		return returnValue;
	}
	MutextedWrapper() = default;
	MutextedWrapper(std::shared_ptr<T>&& aPtr)
	{
		ptr.swap(aPtr);
	}
	MutextedWrapper(MutextedWrapper& aOther)
	{
		operator=(aOther.load());
	}
	MutextedWrapper(MutextedWrapper&& aOther)
	{
		operator=(std::move(aOther));
	}
	std::mutex lock;

	MutextedWrapper& operator=(std::shared_ptr<T>& aPtr)
	{
		lock.lock();
		ptr = aPtr;
		lock.unlock();

		return *this;
	}
	MutextedWrapper& operator=(std::shared_ptr<T>&& aPtr)
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
class Tester
{
public:
	template <class ...InitArgs>
	Tester(bool aDoInitializeArray = true, InitArgs && ...aArgs);
	~Tester();

	float Execute(std::uint32_t aArrayPasses, bool aDoAssign = true, bool aDoReassign = true, bool aDoCASTest = true, bool aDoReferenceTest = true);


	void WorkAssign(std::uint32_t aArrayPasses);
	void WorkReassign(std::uint32_t aArrayPasses);
	void WorkReferenceTest(std::uint32_t aArrayPasses);
	void WorkCAS(std::uint32_t aArrayPasses);

	void CheckPointers() const;

	ThreadPool myWorker;

#ifdef ASP_MUTEX_COMPARE
	MutextedWrapper<T> myTestArray[ArraySize];
#else
	atomic_shared_ptr<T> myTestArray[ArraySize];
	ReferenceComparison<T> myReferenceComparison[ArraySize];
#endif
	std::atomic_bool myWorkBlock;

	std::atomic<size_t> mySummary;

	std::random_device myRd;
	std::mt19937 myRng;
};

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
template<class ...InitArgs>
inline Tester<T, ArraySize, NumThreads>::Tester(bool aDoInitializeArray, InitArgs && ...aArgs)
	: myWorker(NumThreads)
	, myRng(myRd())
#ifndef ASP_MUTEX_COMPARE
	, myReferenceComparison{ nullptr }
#endif
{
	if (aDoInitializeArray) {
		for (std::uint32_t i = 0; i < ArraySize; ++i) {
#ifndef ASP_MUTEX_COMPARE
			myTestArray[i] = make_shared<T>(std::forward<InitArgs&&>(aArgs)...);
			myReferenceComparison[i].m_ptr = new T(std::forward<InitArgs&&>(aArgs)...);
#else
			myTestArray[i] = std::make_shared<T>(std::forward<InitArgs&&>(aArgs)...);
#endif
		}
	}
}

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline Tester<T, ArraySize, NumThreads>::~Tester()
{
#ifndef ASP_MUTEX_COMPARE
	for (std::uint32_t i = 0; i < ArraySize; ++i) {
		delete myReferenceComparison[i].m_ptr;
	}
#endif
}

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline float Tester<T, ArraySize, NumThreads>::Execute(std::uint32_t aArrayPasses, bool aDoAssign, bool aDoReassign, bool aDoCASTest, bool aDoReferenceTest)
{
	myWorkBlock.store(false);

	for (std::uint32_t thread = 0; thread < NumThreads; ++thread) {
		if (aDoAssign) {
			myWorker.AddTask([this, aArrayPasses]() { WorkAssign(aArrayPasses); });
		}
		if (aDoReassign) {
			myWorker.AddTask([this, aArrayPasses]() { WorkReassign(aArrayPasses); });
		}
		if (aDoReferenceTest) {
			myWorker.AddTask([this, aArrayPasses]() { WorkReferenceTest(aArrayPasses); });
		}
		if (aDoCASTest) {
			myWorker.AddTask([this, aArrayPasses]() { WorkCAS(aArrayPasses); });
		}
	}

	Timer timer;

	myWorkBlock.store(true);

	while (myWorker.HasUnfinishedTasks()) {
		std::this_thread::yield();
	}

	CheckPointers();

	std::cout << "Checksum" << mySummary << std::endl;

	return timer.GetTotalTime();
}

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline void Tester<T, ArraySize, NumThreads>::WorkAssign(std::uint32_t aArrayPasses)
{
	while (!myWorkBlock) {
		std::this_thread::yield();
	}

	for (std::uint32_t pass = 0; pass < aArrayPasses; ++pass) {
		for (std::uint32_t i = 0; i < ArraySize; ++i) {
			myTestArray[i]
#ifdef ASP_MUTEX_COMPARE
				= std::make_shared<T>();
#else
				.store(make_shared<T>(), std::memory_order_relaxed);
#endif
		}
	}
}

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline void Tester<T, ArraySize, NumThreads>::WorkReassign(std::uint32_t aArrayPasses)
{
	while (!myWorkBlock) {
		std::this_thread::yield();
	}

	for (std::uint32_t pass = 0; pass < aArrayPasses; ++pass) {
		for (std::uint32_t i = 0; i < ArraySize; ++i) {
#ifndef ASP_MUTEX_COMPARE
			myTestArray[i].store(myTestArray[(i + myRng()) % ArraySize].load(std::memory_order_relaxed), std::memory_order_relaxed);
#else
			myTestArray[i] = myTestArray[(i + myRng()) % ArraySize].load(std::memory_order::memory_order_acquire);
#endif
		}
	}

}
template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline void Tester<T, ArraySize, NumThreads>::WorkReferenceTest(std::uint32_t aArrayPasses)
{
#ifndef ASP_MUTEX_COMPARE
	while (!myWorkBlock) {
		std::this_thread::yield();
	}

	size_t localSum(0);

	for (std::uint32_t pass = 0; pass < aArrayPasses; ++pass) {
		for (std::uint32_t i = 0; i < ArraySize; ++i) {
			localSum += *myReferenceComparison[i].m_ptr;
			//localSum += *myTestArray[i];
		}
	}
	mySummary += localSum;
#else
	aArrayPasses;
#endif
}

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline void Tester<T, ArraySize, NumThreads>::WorkCAS(std::uint32_t aArrayPasses)
{

	while (!myWorkBlock) {
		std::this_thread::yield();
	}

	size_t localSum(0);

	for (std::uint32_t pass = 0; pass < aArrayPasses; ++pass) {
		for (std::uint32_t i = 0; i < ArraySize; ++i) {

#ifndef ASP_MUTEX_COMPARE
			shared_ptr<T> desired(make_shared<T>());
			shared_ptr<T> expected(myTestArray[i].load(std::memory_order_relaxed));
			raw_ptr<T> check(expected);
			const bool resulta = myTestArray[i].compare_exchange_strong(expected, std::move(desired), std::memory_order_relaxed);

			shared_ptr<T> desired_(make_shared<T>());
			shared_ptr<T> expected_(myTestArray[i].load(std::memory_order_relaxed));
			raw_ptr<T> rawExpected(expected_);
			raw_ptr<T> check_(expected_);
			const bool resultb = myTestArray[i].compare_exchange_strong(rawExpected, std::move(desired_), std::memory_order_relaxed);

			if (!(resultb == (rawExpected == check_))) {
				throw std::runtime_error("output from expected do not correspond to CAS results");
			}

#else
			std::shared_ptr<T> desired_(std::make_shared<T>());
			std::shared_ptr<T> expected_(myTestArray[i].load(std::memory_order_acquire));
			const bool resultb = myTestArray[i].compare_exchange_strong(expected_, std::move(desired_), std::memory_order_acquire);
#endif
		}
	}

	mySummary += localSum;
}

template<class T, std::uint32_t ArraySize, std::uint32_t NumThreads>
inline void Tester<T, ArraySize, NumThreads>::CheckPointers() const
{
#ifndef ASP_MUTEX_COMPARE
	//uint32_t count(0);
	//for (std::uint32_t i = 0; i < ArraySize; ++i) {
	//	const gdul::aspdetail::control_block_base<T, gdul::aspdetail::default_allocator>* const controlBlock(myTestArray[i].unsafe_get_control_block());
	//	const T* const directObject(myTestArray[i].unsafe_get_owned());
	//	const T* const sharedObject(controlBlock->get_owned());
	//
	//	if (directObject != sharedObject) {
	//		++count;
	//	}
	//}
	//std::cout << "Mismatch shared / object count: " << count << std::endl;
#endif
}
