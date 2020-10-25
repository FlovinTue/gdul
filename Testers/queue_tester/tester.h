#pragma once

#include <thread>
#include "../Common/thread_pool.h"
#include <gdul\concurrent_queue\concurrent_queue.h>
#include <concurrent_priority_queue.h>
#include "../Common/Timer.h"
#include <concurrent_queue.h>
#include "../Common/tracking_allocator.h"
#include "../Common/util.h"
#include <limits>
#include <string>

#if defined(GDUL_FIFO)
#include <gdul/WIP_concurrent_queue_fifo/concurrent_queue_fifo_v6.h>
#elif defined(GDUL_CPQ)
#include <gdul/WIP_concurrent_priority_queue/concurrent_priority_queue_v10.h>
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

const std::uint32_t Writes = 4;
const std::uint32_t Writers = std::thread::hardware_concurrency() / 2;
const std::uint32_t Readers = std::thread::hardware_concurrency() / 2;
const std::uint32_t WritesPerThread(Writes / Writers);
const std::uint32_t ReadsPerThread(Writes / Readers);

namespace gdul {
enum : std::uint32_t
{
	test_option_concurrent = 1 << 0,
	test_option_single = 1 << 1,
	test_option_singleReadWrite = 1 << 2,
	test_option_onlyRead = 1 << 3,
	test_option_onlyWrite = 1 << 4,
	test_option_all = std::numeric_limits<std::uint32_t>::max(),
};

template <class T, class Allocator>
void queue_testrun(std::uint32_t runs, Allocator alloc, std::uint32_t options = test_option_all) {
	tester<T, Allocator> tester(alloc);
	
	for (std::uint32_t i = 0; i < runs; ++i) {
		std::cout << "Pre-run alloc value is: " << gdul::s_allocated << std::endl;

		double concurrentRes(0.0);
		double singleRes(0.0);
		double writeRes(0.0);
		double readRes(0.0);
		double singleProdSingleCon(0.0);

#if defined(_DEBUG)
		// Arbitrary value. Works well
		const int iterations = 100;
#else
		// Arbitrary value. Works well
		const int iterations = 1000;
#endif

		if (options & test_option_singleReadWrite)
			singleProdSingleCon = tester.ExecuteSingleProducerSingleConsumer(iterations);
		if (options & test_option_onlyWrite)
			writeRes = tester.ExecuteWrite(iterations);
		if (options & test_option_onlyRead)
			readRes = tester.ExecuteRead(iterations);
		if (options & test_option_single)
			singleRes = tester.ExecuteSingleThread(iterations);
		if (options & test_option_concurrent)
			concurrentRes = tester.ExecuteConcurrent(iterations);

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
			std::string("Concurrent (") + std::to_string(Writers) + std::string("Writers, ") + std::to_string(Readers) + std::string("Readers): ") +
			std::string(std::to_string(concurrentRes / static_cast<double>(iterations))) + std::string("			// total: ") + std::to_string(concurrentRes) +
			std::string("s\n") +
			std::string("Concurrent (") + std::to_string(1) + std::string("Writers, ") + std::to_string(1) + std::string("Readers): ") +
			std::string(std::to_string(singleProdSingleCon / static_cast<double>(iterations))) + std::string("			// total: ") + std::to_string(singleProdSingleCon) +
			std::string("s\n") +
			std::string("Single thread: ") +
			std::string(std::to_string(singleRes / static_cast<double>(iterations))) + std::string("						// total: ") + std::to_string(singleRes) +
			std::string("s\n") +
			std::string("Pure writes: (") + std::to_string(Writers) + std::string("): ") +
			std::string(std::to_string(writeRes / static_cast<double>(iterations))) + std::string("					// total: ") + std::to_string(writeRes) +
			std::string("s\n") +
			std::string("Pure reads: (") + std::to_string(Readers) + std::string("): ") +
			std::string(std::to_string(readRes / static_cast<double>(iterations))) + std::string("					// total: ") + std::to_string(readRes) +
			std::string("s\n") +
			std::string("Total time is: " + std::to_string(concurrentRes + singleRes + writeRes + readRes + singleProdSingleCon) + "\n") +
			std::string("per " + std::to_string(Writes) + " operations in ") +
			config);

		std::cout << str << "\n" << std::endl;

		std::cout << "Post-run alloc value is: " << gdul::s_allocated << std::endl;
	}
}

template <class T, class Allocator>
class tester
{
public:
	tester(Allocator& alloc);
	~tester();

	double ExecuteConcurrent(std::uint32_t runs);
	double ExecuteSingleThread(std::uint32_t runs);
	double ExecuteSingleProducerSingleConsumer(std::uint32_t runs);
	double ExecuteRead(std::uint32_t runs);
	double ExecuteWrite(std::uint32_t runs);

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
	concurrent_priority_queue<typename T::first_type, typename T::second_type> m_queue;
	static thread_local gdul::shared_ptr<typename decltype(m_queue)::node_type[]> m_nodes;

	static thread_local std::vector<typename decltype(m_queue)::node_type*> t_output;
#elif defined(MS_CPQ)
	concurrency::concurrent_priority_queue<T> m_queue;
#endif


#if !defined(GDUL_CPQ)
	static thread_local std::vector<T> t_output;
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
	m_writer(Writers, 0),
	m_reader(Readers, Writers),
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
inline double tester<T, Allocator>::ExecuteConcurrent(std::uint32_t runs) {
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
		m_waiting = 0;

		result += time.get();

		m_isRunning = false;
	}

	std::cout << "ExecuteConcurrent Threw " << m_thrown;
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
	}

	std::cout << "ExecuteSingleThread Threw " << m_thrown;
	if (!CheckResults()) {
		std::cout << " and failed check (w" << m_writtenSum << " / r" << m_readSum << ")";
	}

	std::cout << std::endl;
	return result;
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteSingleProducerSingleConsumer(std::uint32_t runs) {
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

	}

	std::cout << "ExecuteSingleProducerSingleConsumer Threw " << m_thrown;
	if (!CheckResults()) {
		std::cout << " and failed check (w" << m_writtenSum << " / r" << m_readSum << ")";
	}
	std::cout << std::endl;

	return result;
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteRead(std::uint32_t runs) {
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

#if defined GDUL_CPQ
		GDUL_ASSERT(m_queue.size() == ReadsPerThread * Readers);
#endif

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
	}

	std::cout << "ExecuteRead Threw " << m_thrown;
	if (!CheckResults()) {
		std::cout << " and failed check (w" << m_writtenSum << " / r" << m_readSum << ")";
	}

	std::cout << std::endl;
	return result;
}
template<class T, class Allocator>
inline double tester<T, Allocator>::ExecuteWrite(std::uint32_t runs) {
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
#elif defined(MSC_RUNTIME) || defined(GDUL_CPQ)
		m_queue.clear();
#elif defined(MOODYCAMEL)
		T out;
		while (m_queue.try_dequeue(out));
#endif
	}

	std::cout << "ExecuteWrite Threw " << m_thrown << std::endl;

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
#elif defined(GDUL_CPQ)
	if (!m_nodes) {
		m_nodes = gdul::make_shared<typename decltype(m_queue)::node_type[]>(writes);
	}
#if defined GDUL_CPQ_DEBUG
	std::memset(m_nodes.get(), 0, m_nodes.item_count() * sizeof(typename decltype(m_nodes)::decayed_type));
#endif
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
#elif !defined(GDUL_CPQ)
		T in;
		in.count = seed % (j + 1);
		//in.count = 1;
		sum += in.count;
#endif

#if !defined(MOODYCAMEL) && !defined(GDUL_CPQ)
		m_queue.push(in);
#elif defined(GDUL_CPQ)
		m_nodes[j].m_kv.first = seed & (j + 1);
		m_queue.push(&m_nodes[j]); 
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

#if defined GDUL_CPQ_DEBUG
	t_output.clear();
	t_output.reserve(reads);
#endif

	while (m_waiting < (Writers + Readers)) {
		std::this_thread::yield();
	}

	while (!m_isRunning)
		std::this_thread::yield();

	uint32_t sum(0);

#if !defined(GDUL_CPQ)
	T out{ 0 };
#else
	typename decltype(m_queue)::node_type* out(nullptr);
#endif
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
#endif
#if defined GDUL_CPQ_DEBUG
				t_output.push_back(out);
#endif
				break;
			}
		}
	}
#endif
	m_readSum += sum;
}
}