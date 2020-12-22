#pragma once

#include <thread>
#include "../Common/thread_pool.h"
#include <gdul\concurrent_queue\concurrent_queue.h>
#include <concurrent_priority_queue.h>
#include "../Common/Timer.h"
#include <concurrent_queue.h>
#include "../Common/tracking_allocator.h"
#include "../Common/util.h"
#include "../Common/watch_dog.h"
#include <limits>
#include <string>

#if defined(GDUL_FIFO)
#include <gdul/WIP_concurrent_queue_fifo/concurrent_queue_fifo_v6.h>
#elif defined(GDUL_CPQ)
#include <gdul/WIP_concurrent_priority_queue/concurrent_priority_queue_v16.h>
#endif
#include <queue>
#include <mutex>
#include <random>

// Test queue

#ifdef MOODYCAMEL
#include <concurrentqueue.h>
#endif

#undef max

namespace gdul {
#if !defined(GDUL_CPQ)
extern std::random_device rd;
extern std::mt19937 rng;
#endif

template <class T, class Allocator>
class tester;
}

template <class T>
class queue_mutex_wrapper
{
public:

	bool try_pop(T& out) {
		mtx.lock();
		if (m_queue.empty()) {
			mtx.unlock();
			return false;
		}
		out = m_queue.front();
		m_queue.pop();
		mtx.unlock();
		return true;
	}
	void push(T& in) { mtx.lock(); m_queue.push(in); mtx.unlock(); }
	void clear() {
		mtx.lock();
		while (!m_queue.empty())m_queue.pop();
		mtx.unlock();
	}

	std::mutex mtx;
	std::queue<T> m_queue;
};

const std::uint32_t Writes = 1024;
const std::uint32_t Writers = std::thread::hardware_concurrency() / 2;
const std::uint32_t Readers = std::thread::hardware_concurrency() / 2;
const std::uint32_t WritesPerThread(Writes / Writers);
const std::uint32_t ReadsPerThread(Writes / Readers);

namespace gdul {
enum : std::uint32_t
{
	test_option_mpmc = 1 << 0,
	test_option_single = 1 << 1,
	test_option_spsc = 1 << 2,
	test_option_mc = 1 << 3,
	test_option_mp = 1 << 4,
	test_option_spmc = 1 << 5,
	test_option_mpsc = 1 << 6,
	test_option_all = std::numeric_limits<std::uint32_t>::max(),
};

template <class T, class Allocator>
void queue_testrun(std::uint32_t runs, Allocator alloc, std::uint32_t options = test_option_all) {
	tester<T, Allocator> tester(alloc);
	
	static watch_dog watchDog;

	watchDog.give_instruction(1000 * 25);

	for (std::uint32_t i = 0; i < runs; ++i) {
		std::cout << "Pre-run alloc value is: " << gdul::s_allocated.load() << std::endl;

		double concurrentRes(0.0);
		double singleRes(0.0);
		double writeRes(0.0);
		double readRes(0.0);
		double singleProdSingleCon(0.0);
		double spmcRes(0.0);
		double mpscRes(0.0);

#if defined(_DEBUG)
		// Arbitrary value. Works well
		const int iterations = 100;
#else
		// Arbitrary value. Works well
		const int iterations = 1000;
#endif

		watchDog.pet();

		if (options & test_option_spsc)
			singleProdSingleCon = tester.ExecuteSPSC(iterations);
		if (options & test_option_mp)
			writeRes = tester.ExecuteMP(iterations);
		if (options & test_option_mc)
			readRes = tester.ExecuteMC(iterations);
		if (options & test_option_single)
			singleRes = tester.ExecuteSingleThread(iterations);
		if (options & test_option_mpmc)
			concurrentRes = tester.ExecuteMPMC(iterations);
		if (options & test_option_spmc)
			spmcRes = tester.ExecuteSPMC(iterations);
		if (options & test_option_mpsc)
			mpscRes = tester.ExecuteMPSC(iterations);


		std::cout << "\n";
#ifdef _DEBUG
		std::string config("Debug mode");
#else
		std::string config("Release mode");
#endif
		std::string str = std::string(
			std::string("Executed a total of ") +
			std::to_string(Writes * iterations) +
			std::string(" read & write operations") +
			std::string("\n") +
			std::string("Averaging results \n") +
			std::string("MPMC (") + std::to_string(Writers) + std::string(", ") + std::to_string(Readers) + std::string("): ") +
			std::string(std::to_string(concurrentRes / static_cast<double>(iterations))) + std::string("			// total: ") + std::to_string(concurrentRes) +
			std::string("s\n") +
			std::string("SPSC (") + std::to_string(1) + std::string(", ") + std::to_string(1) + std::string("): ") +
			std::string(std::to_string(singleProdSingleCon / static_cast<double>(iterations))) + std::string("			// total: ") + std::to_string(singleProdSingleCon) +
			std::string("s\n") +
			std::string("Single thread: ") +
			std::string(std::to_string(singleRes / static_cast<double>(iterations))) + std::string("			// total: ") + std::to_string(singleRes) +
			std::string("s\n") +
			std::string("MP: (") + std::to_string(Writers) + std::string("): ") +
			std::string(std::to_string(writeRes / static_cast<double>(iterations))) + std::string("			// total: ") + std::to_string(writeRes) +
			std::string("s\n") +
			std::string("MC: (") + std::to_string(Readers) + std::string("): ") +
			std::string(std::to_string(readRes / static_cast<double>(iterations))) + std::string("			// total: ") + std::to_string(readRes) +
			std::string("s\n") +
			std::string("SPMC (") + "1" + std::string(", ") + std::to_string(Readers) + std::string("): ") +
			std::string(std::to_string(spmcRes / static_cast<double>(iterations))) + std::string("			// total: ") + std::to_string(spmcRes) +
			std::string("s\n") +
			std::string("MPSC (") + std::to_string(Writers) + std::string(", ") + "1" + std::string("): ") +
			std::string(std::to_string(mpscRes / static_cast<double>(iterations))) + std::string("			// total: ") + std::to_string(mpscRes) +
			std::string("s\n") +
			std::string("Total time is: " + std::to_string(concurrentRes + singleRes + writeRes + readRes + singleProdSingleCon + spmcRes + mpscRes) + "\n") +
			std::string("per " + std::to_string(Writes) + " operations in ") +
			config);

		std::cout << str << "\n" << std::endl;

		std::cout << "Post-run alloc value is: " << gdul::s_allocated.load() << std::endl;
	}
}

template <class T, class Allocator>
class tester
{
public:
	tester(Allocator& alloc);
	~tester();

	double ExecuteMPMC(std::uint32_t runs);
	double ExecuteSingleThread(std::uint32_t runs);
	double ExecuteSPSC(std::uint32_t runs);
	double ExecuteMC(std::uint32_t runs);
	double ExecuteMP(std::uint32_t runs);
	double ExecuteSPMC(std::uint32_t runs);
	double ExecuteMPSC(std::uint32_t runs);

private:
	bool CheckResults() const;

	void Write(std::uint32_t writes);
	void Read(std::uint32_t writes);

#if defined( GDUL)
	gdul::concurrent_queue<T, Allocator> m_queue;
#elif defined(GDUL_FIFO)
	gdul::concurrent_queue_fifo<T, Allocator> m_queue;
#elif defined(MSC_RUNTIME)
	concurrency::concurrent_queue<T> m_queue;
#elif defined(MOODYCAMEL)
	moodycamel::ConcurrentQueue<T> m_queue;
#elif defined(MTX_WRAPPER)
	queue_mutex_wrapper<T> m_queue;
#elif defined(GDUL_CPQ)
	concurrent_priority_queue<typename T::first_type, typename T::second_type, 512, gdul::cpq_allocation_strategy_pool<std::allocator<std::uint8_t>>> m_queue;
#elif defined(MS_CPQ)
	concurrency::concurrent_priority_queue<T> m_queue;
#endif

	gdul::thread_pool m_writer;
	gdul::thread_pool m_reader;

	std::atomic<bool> m_isRunning;
	std::atomic<std::uint32_t> m_writtenSum;
	std::atomic<std::uint32_t> m_readSum;
	std::atomic<std::uint32_t> m_thrown;
	std::atomic<std::uint32_t> m_waiting;
};

template<class T, class Allocator>
inline tester<T, Allocator>::tester(Allocator&
#ifdef GDUL
	alloc
#endif
) :
	m_isRunning(false),
	m_writer((1 << 0) | (1 << 2) | (1 << 4) | (1 << 6)),
	m_reader((1 << 1) | (1 << 3) | (1 << 5) | (1 << 7)),
	m_writtenSum(0),
	m_readSum(0),
	m_thrown(0),
	m_waiting(0)
#ifdef GDUL
	, m_queue(alloc)
#endif
{
	srand(static_cast<std::uint32_t>(time(0)));
}

template<class T, class Allocator>
inline tester<T, Allocator>::~tester() {
	m_writer.decommission();
	m_reader.decommission();
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteMPMC(std::uint32_t runs) {
#if defined(GDUL) | defined(GDUL_FIFO)
	m_queue.unsafe_reset();
#endif

	double result(0.0);
	m_thrown = 0;
	m_writtenSum = 0;
	m_readSum = 0;

#if defined(GDUL_FIFO)
	m_queue.reserve(Writes);
#endif

	for (std::uint32_t i = 0; i < runs; ++i) {

		for (std::uint32_t j = 0; j < Writers; ++j)
			m_writer.add_task(std::bind(&tester::Write, this, WritesPerThread));
		for (std::uint32_t j = 0; j < Readers; ++j)
			m_reader.add_task(std::bind(&tester::Read, this, ReadsPerThread));

		gdul::timer<float> time;
		m_isRunning = true;

		while (m_writer.has_unfinished_tasks() | m_reader.has_unfinished_tasks())
			std::this_thread::yield();

#if defined(GDUL) | defined(GDUL_FIFO)
		m_queue.unsafe_clear();
#elif defined(MSC_RUNTIME) || defined(MTX_WRAPPER) || defined(GDUL_CPQ)
		m_queue.clear();
#endif

#if defined GDUL_CPQ
		//m_queue.unsafe_reset_scratch_pool();
#endif
		m_waiting = 0;

		result += time.get();

		m_isRunning = false;
	}

	std::cout << "ExecuteMPMC Threw " << m_thrown;
	if (!CheckResults()) {
		std::cout << " and failed check (w" << m_writtenSum << " / r" << m_readSum << ")";
	}
	std::cout << std::endl;

	return result;
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteSPMC(std::uint32_t runs)
{
#if defined(GDUL) | defined(GDUL_FIFO)
	m_queue.unsafe_reset();
#endif

	double result(0.0);
	m_thrown = 0;
	m_writtenSum = 0;
	m_readSum = 0;

#if defined(GDUL_FIFO)
	m_queue.reserve(Writes);
#endif

	for (std::uint32_t i = 0; i < runs; ++i) {
		
		m_waiting = Writers - 1;

		for (std::uint32_t j = 0; j < Readers; ++j)
			m_reader.add_task(std::bind(&tester::Read, this, ReadsPerThread));
			
		m_writer.add_task(std::bind(&tester::Write, this, Writes));

		gdul::timer<float> time;
		m_isRunning = true;

		while (m_writer.has_unfinished_tasks() | m_reader.has_unfinished_tasks())
			std::this_thread::yield();

#if defined(GDUL) | defined(GDUL_FIFO)
		m_queue.unsafe_clear();
#elif defined(MSC_RUNTIME) || defined(MTX_WRAPPER) || defined(GDUL_CPQ)
		m_queue.clear();
#endif

#if defined GDUL_CPQ
		//m_queue.unsafe_reset_scratch_pool();
#endif
		m_waiting = 0;

		result += time.get();

		m_isRunning = false;
	}

	std::cout << "ExecuteSPMC Threw " << m_thrown;
	if (!CheckResults()) {
		std::cout << " and failed check (w" << m_writtenSum << " / r" << m_readSum << ")";
	}
	std::cout << std::endl;

	return result;
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteMPSC(std::uint32_t runs)
{
#if defined(GDUL) | defined(GDUL_FIFO)
	m_queue.unsafe_reset();
#endif

	double result(0.0);
	m_thrown = 0;
	m_writtenSum = 0;
	m_readSum = 0;

#if defined(GDUL_FIFO)
	m_queue.reserve(Writes);
#endif

	for (std::uint32_t i = 0; i < runs; ++i) {

		m_waiting = Readers - 1;

		for (std::uint32_t j = 0; j < Readers; ++j)
			m_writer.add_task(std::bind(&tester::Write, this, WritesPerThread));
	
		m_reader.add_task(std::bind(&tester::Read, this, ReadsPerThread * Readers));

		gdul::timer<float> time;
		m_isRunning = true;

		while (m_writer.has_unfinished_tasks() | m_reader.has_unfinished_tasks())
			std::this_thread::yield();

#if defined(GDUL) | defined(GDUL_FIFO)
		m_queue.unsafe_clear();
#elif defined(MSC_RUNTIME) || defined(MTX_WRAPPER) || defined(GDUL_CPQ)
		m_queue.clear();
#endif

#if defined GDUL_CPQ
		//m_queue.unsafe_reset_scratch_pool();
#endif
		m_waiting = 0;

		result += time.get();

		m_isRunning = false;
	}

	std::cout << "ExecuteMPSC Threw " << m_thrown;
	if (!CheckResults()) {
		std::cout << " and failed check (w" << m_writtenSum << " / r" << m_readSum << ")";
	}
	std::cout << std::endl;

	return result;
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteSingleThread(std::uint32_t runs) {
#if  defined(GDUL) | defined(GDUL_FIFO)
	m_queue.unsafe_reset();
#endif
	double result(0.0);
	m_thrown = 0;
	m_writtenSum = 0;
	m_readSum = 0;

#if defined(GDUL_FIFO)
	m_queue.reserve(Writes);
#endif

	for (std::uint32_t i = 0; i < runs; ++i) {
		m_waiting = Readers + Writers;

		m_isRunning = true;

		gdul::timer<float> time;

		Write(Writes);
		Read(Writes);

		result += time.get();

		m_waiting = 0;

		m_isRunning = false;

#if defined(GDUL) | defined(GDUL_FIFO)
		m_queue.unsafe_clear();
#elif defined(MSC_RUNTIME) || defined(MTX_WRAPPER) || defined(GDUL_CPQ)
		m_queue.clear();
#endif

#if defined GDUL_CPQ
		//m_queue.unsafe_reset_scratch_pool();
#endif

	}

	std::cout << "ExecuteSingleThread Threw " << m_thrown;
	if (!CheckResults()) {
		std::cout << " and failed check (w" << m_writtenSum << " / r" << m_readSum << ")";
	}

	std::cout << std::endl;
	return result;
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteSPSC(std::uint32_t runs) {
#if  defined(GDUL) | defined(GDUL_FIFO)
	m_queue.unsafe_reset();
#endif

#if defined(GDUL_FIFO)
	m_queue.reserve(runs);
#endif

	double result(0.0);
	m_thrown = 0;
	m_writtenSum = 0;
	m_readSum = 0;

	for (std::uint32_t i = 0; i < runs; ++i) {

		m_waiting = Readers + Writers - 2;

		m_writer.add_task(std::bind(&tester::Write, this, Writes));
		m_reader.add_task(std::bind(&tester::Read, this, Writes));

		gdul::timer<float> time;

		m_isRunning = true;

		while (m_writer.has_unfinished_tasks() | m_reader.has_unfinished_tasks())
			std::this_thread::yield();

		result += time.get();

		m_waiting = 0;

		m_isRunning = false;
#if defined(GDUL) | defined(GDUL_FIFO)
		m_queue.unsafe_clear();
#elif defined(MSC_RUNTIME) || defined(MTX_WRAPPER) || defined(GDUL_CPQ)
		m_queue.clear();
#endif

#if defined GDUL_CPQ
		//m_queue.unsafe_reset_scratch_pool();
#endif

	}

	std::cout << "ExecuteSPSC Threw " << m_thrown;
	if (!CheckResults()) {
		std::cout << " and failed check (w" << m_writtenSum << " / r" << m_readSum << ")";
	}
	std::cout << std::endl;

	return result;
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteMC(std::uint32_t runs) {
#if  defined(GDUL) | defined(GDUL_FIFO)
	m_queue.unsafe_reset();
#endif

#if defined(GDUL_FIFO)
	m_queue.reserve(Writes);
#endif

	double result(0.0);
	m_thrown = 0;
	m_writtenSum = 0;
	m_readSum = 0;

	for (std::uint32_t i = 0; i < runs; ++i) {

		m_waiting = Readers + Writers;

		m_isRunning = true;

		Write(ReadsPerThread * Readers);

		m_isRunning = false;

		m_waiting = Writers;

		for (std::uint32_t j = 0; j < Readers; ++j)
			m_reader.add_task(std::bind(&tester::Read, this, ReadsPerThread));

		gdul::timer<float> time;
		m_isRunning = true;

		while (m_reader.has_unfinished_tasks())
			std::this_thread::yield();

		result += time.get();

		m_waiting = 0;

		m_isRunning = false;

#if defined(GDUL) | defined(GDUL_FIFO)
		m_queue.unsafe_clear();
#elif defined(MSC_RUNTIME) || defined(MTX_WRAPPER) || defined(GDUL_CPQ)
		m_queue.clear();
#endif

#if defined GDUL_CPQ
		//m_queue.unsafe_reset_scratch_pool();
#endif
	}

	std::cout << "ExecuteMC Threw " << m_thrown;
	if (!CheckResults()) {
		std::cout << " and failed check (w" << m_writtenSum << " / r" << m_readSum << ")";
	}

	std::cout << std::endl;
	return result;
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteMP(std::uint32_t runs) {
#if  defined(GDUL) | defined(GDUL_FIFO)
	m_queue.unsafe_reset();
#endif

#if defined(GDUL_FIFO)
	m_queue.reserve(Writes);
#endif

	double result(0.0);
	m_thrown = 0;
	m_writtenSum = 0;
	m_readSum = 0;

	for (std::uint32_t i = 0; i < runs; ++i) {

		m_waiting = Writers;

		for (std::uint32_t j = 0; j < Writers; ++j)
			m_writer.add_task(std::bind(&tester::Write, this, WritesPerThread));

		gdul::timer<float> time;
		m_isRunning = true;

		while (m_writer.has_unfinished_tasks())
			std::this_thread::yield();

		result += time.get();

		m_waiting = 0;

		m_isRunning = false;

#if  defined(GDUL) | defined(GDUL_FIFO)
		m_queue.unsafe_clear();
#elif defined(MSC_RUNTIME) || defined(GDUL_CPQ) || defined(GDUL_CPQ)
		m_queue.clear();
#elif defined(MOODYCAMEL)
		T out;
		while (m_queue.try_dequeue(out));
#endif

#if defined GDUL_CPQ
		//m_queue.unsafe_reset_scratch_pool();
#endif
	}

	std::cout << "ExecuteMP Threw " << m_thrown << std::endl;

	return result;
}

template<class T, class Allocator>
inline bool tester<T, Allocator>::CheckResults() const {
	if (m_writtenSum != m_readSum)
		return false;

	return true;
}
template<class T, class Allocator>
inline void tester<T, Allocator>::Write(std::uint32_t writes) {
#ifdef GDUL
	/*m_queue.reserve(Writes);*/
#endif

	++m_waiting;

	while (m_waiting < (Writers + Readers)) {
		std::this_thread::yield();
	}

	while (!m_isRunning)
		std::this_thread::yield();

	uint32_t sum(0);
	uint32_t seed(rand());

#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	for (std::uint32_t j = 0; j < writes; ) {
		T in(seed % (j + 1));
		try {
			m_queue.push(in);
			++j;
			sum += in.count;
		}
		catch (...) {
			++m_thrown;
		}
	}
#else
	for (std::uint32_t j = 0; j < writes; ++j) {

#if defined(MS_CPQ)
		T in = seed % (j + 1);
		sum += in;
#elif !defined(GDUL_CPQ)
		T in;
		in.count = seed % (j + 1);
		//in.count = 1;
		sum += in.count;
#endif

#if !defined(MOODYCAMEL) && !defined(GDUL_CPQ)
		m_queue.push(in);
#elif defined(GDUL_CPQ)
		const std::pair<typename T::first_type, typename T::second_type> in(seed % (j + 1), typename T::second_type());
		m_queue.push(in);
		sum += in.first;
#else
		m_queue.enqueue(in);
#endif
	}
#endif

#if defined _DEBUG
	while (m_reader.has_unfinished_tasks()) {
		std::this_thread::yield();
	}
#endif

	m_writtenSum += sum;
}

template<class T, class Allocator>
inline void tester<T, Allocator>::Read(std::uint32_t reads) {
	++m_waiting;

	while (m_waiting < (Writers + Readers)) {
		std::this_thread::yield();
	}

	while (!m_isRunning)
		std::this_thread::yield();

	uint32_t sum(0);

	T out;

#ifdef GDUL_CQ_ENABLE_EXCEPTIONHANDLING
	for (std::uint32_t j = 0; j < reads;) {
		while (true) {
			try {
				if (m_queue.try_pop(out)) {
					++j;
					sum += out.count;
					break;
				}
			}
			catch (...) {
				++m_thrown;
			}
		}
	}

#else
	for (std::uint32_t j = 0; j < reads; ++j) {
		while (true) {
#ifndef MOODYCAMEL
			if (m_queue.try_pop(out)) {
#else
			if (m_queue.try_dequeue(out)) {
#endif
#if !defined(GDUL_CPQ) && ! defined(MS_CPQ)
				sum += out.count;

#elif defined (MS_CPQ)
				sum += out;
#elif defined (GDUL_CPQ)
				sum += out.first;
#endif
				break;
			}
		}
	}
#endif
	m_readSum += sum;
}
}