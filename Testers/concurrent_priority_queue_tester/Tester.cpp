#include "stdafx.h"
#include "CppUnitTest.h"
#include <gdul\WIP_concurrent_priority_queue\concurrent_priority_queue.h>
#include <gdul\concurrent_queue\concurrent_queue.h>
#include <thread>
#include <random>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

namespace Tester
{
TEST_CLASS(UnitTest1)
{
public:

	//TEST_METHOD(insert) {
	//	gdul::concurrent_priority_queue<uint64_t, int, 2> list;

	//	list.insert({2, 2});
	//	list.insert({1, 1});

	//	uint64_t key(0);

	//	Assert::IsTrue(list.size() == 2);
	//}
	//TEST_METHOD(try_peek){
	//	gdul::concurrent_priority_queue<uint64_t, int, 2> list;

	//	uint64_t firstKey(2);
	//	uint64_t secondKey(1);

	//	list.insert({ firstKey, 2 });
	//	list.insert({ secondKey, 1 });

	//	uint64_t key(0);

	//	list.try_peek_top_key(key);

	//	Assert::IsTrue(key == secondKey);
	//}
	//TEST_METHOD(try_pop) {
	//	gdul::concurrent_priority_queue<uint64_t, int, 2> list;

	//	int first(2);
	//	int second(1);
	//	uint64_t firstKey(2);
	//	uint64_t secondKey(1);

	//	list.insert({ firstKey, first });
	//	list.insert({ secondKey, second });

	//	std::pair<uint64_t, int> out1;
	//	std::pair<uint64_t, int> out2;

	//	list.try_pop(out1);
	//	list.try_pop(out2);

	//	Assert::IsTrue(out1.second == second, L"First value out was not equal to second value in");
	//	Assert::IsTrue(out2.second == first, L"Second value out was not equal to first value in");
	//	Assert::IsTrue(out1.first == secondKey, L"First key out was not second key in");
	//	Assert::IsTrue(out2.first == firstKey, L"Second key out was not first key in");
	//	Assert::IsFalse(list.try_pop(out1), L"Did not return empty upon try_pop. List should be empty");
	//}
	//TEST_METHOD(flood_insert) {
	//	gdul::concurrent_priority_queue<uint64_t, int, 8 * 2000> list;

	//	list.insert({ 0, 0 });

	//	uint32_t numInserts(2000);
	//	uint32_t numthreads(8);
	//	gdul::concurrent_queue<uint64_t> que;

	//	{
	//		std::atomic<bool> begin(false);
	//		std::atomic<uint32_t> finished(0);

	//		auto insertLam = [&list, &begin, &finished, numInserts, &que]() {

	//			std::random_device rd;
	//			std::mt19937 rng(rd());

	//			que.reserve(numInserts);

	//			while (!begin)
	//				std::this_thread::yield();

	//			for (uint32_t i = 0; i < numInserts; ++i) {
	//				uint64_t key(rng());
	//				int val(rng());

	//				list.insert({ key, val });
	//				que.push(key);
	//			}

	//			++finished;
	//		};

	//		
	//		for (uint32_t i = 0; i < numthreads; ++i) {
	//			std::thread thread(insertLam);
	//			thread.detach();
	//		}
	//		begin = true;

	//		while (finished.load() != numthreads) {
	//			std::this_thread::sleep_for(std::chrono::microseconds(10));
	//		}
	//	}
	//	heap<int> hep;
	//	hep.push(0, 0);
	//	while (que.size()) {
	//		uint64_t out;
	//		que.try_pop(out);
	//		hep.push(0, out);
	//	}

	//	Assert::IsTrue(list.size() == numthreads * numInserts + 1, L"size does not match after insertions");
	//	
	//	uint64_t topKey(UINT64_MAX);
	//	list.try_peek_top_key(topKey);

	//	Assert::IsTrue(topKey == 0, L"Top key has changed....");

	//	for (uint32_t i = 0; i < numInserts * numthreads; ++i) {
	//		std::pair<uint64_t, int> out;
	//		Assert::IsTrue(list.try_pop(out), L"Failed to pop entry that should be present");
	//		uint64_t heapKey;
	//		int heapOut;
	//		hep.try_pop(heapOut, heapKey);

	//		Assert::IsTrue(out.first == heapKey, L"Keys do not match");
	//	}
	//}
	//TEST_METHOD(flood_pop) {
	//	gdul::concurrent_priority_queue<uint64_t, int, 4000> list;

	//	uint32_t numInserts(500);
	//	uint32_t numthreads(8);
	//	for (uint32_t i = 0; i < numInserts * numthreads; ++i) {
	//		list.insert({ rand(), rand() });
	//	}

	//	// ---------------------------------------------------

	//	{
	//		std::atomic<bool> begin(false);
	//		std::atomic<uint32_t> finished(0);


	//		auto insertLam = [&list, &begin, &finished, numInserts]() {

	//			while (!begin)
	//				std::this_thread::yield();

	//			for (uint32_t i = 0; i < numInserts; ++i) {
	//				std::pair<uint64_t, int> out;
	//				Assert::IsTrue(list.try_pop(out), L"Failed to pop when element should be present");
	//			}

	//			++finished;
	//		};

	//		for (uint32_t i = 0; i < numthreads; ++i) {
	//			std::thread thread(insertLam);
	//			thread.detach();
	//		}
	//		begin = true;

	//		while (finished.load() != numthreads) {
	//			std::this_thread::sleep_for(std::chrono::microseconds(10));
	//		}
	//		int out(0);
	//		Assert::IsFalse(list.try_pop(out), L"List should be empty");
	//		uint64_t key(0);
	//		Assert::IsFalse(list.try_peek_top_key(key), L"List should be empty");
	//		Assert::IsTrue(list.size() == 0, L"Size should output 0");
	//	}
	//}
	//TEST_METHOD(flood_push_pop) {
	//	gdul::concurrent_priority_queue<uint64_t, int, 4000> list;
	//	gdul::concurrent_queue<uint64_t> inQue;
	//	gdul::concurrent_queue<uint64_t> outQue;

	//	std::random_device rd;
	//	std::mt19937 rng(rd());

	//	std::atomic<uint32_t> popCount(0);

	//	uint32_t numOps(500);
	//	uint32_t numthreads(8);

	//	std::atomic<bool> begin(false);
	//	std::atomic<uint32_t> finished(0);

	//	auto lam = [&list, &begin, &finished, numOps, &rng, &popCount, &inQue, &outQue]() {
	//		inQue.reserve(numOps);
	//		outQue.reserve(numOps);

	//		while (!begin)
	//			std::this_thread::yield();

	//		for (uint32_t i = 0; i < numOps; ++i) {
	//			std::pair<uint64_t, int> a(rng(), rng());
	//			std::pair<uint64_t, int> b(rng(), rng());

	//			list.insert(a);
	//			list.insert(b);

	//			std::pair<uint64_t, int> out;
	//			list.try_pop(out);

	//			inQue.push(a.first);
	//			inQue.push(b.first);
	//			outQue.push(out.first);
	//		}

	//		++finished;
	//	};


	//	for (uint32_t i = 0; i < numthreads; ++i) {
	//		std::thread thread(lam);
	//		thread.detach();
	//	}
	//	begin = true;

	//	while (finished.load() != numthreads) {
	//		std::this_thread::sleep_for(std::chrono::microseconds(10));
	//	}

	//	Assert::IsTrue(list.size() == numOps * numthreads, L"Bad size");

	//	uint64_t last(0);
	//	std::pair<uint64_t, int> out;
	//	while (list.try_pop(out)) {
	//		Assert::IsFalse(out.first < last, L"Popped value less than last");
	//		last = out.first;
	//		outQue.push(out.first);
	//	}

	//	heap<int> hepa;
	//	uint64_t outa(0);
	//	while (outQue.try_pop(outa)) {
	//		hepa.push(0, outa);
	//	}

	//	heap<int> hepb;
	//	uint64_t outb(0);
	//	while (inQue.try_pop(outb)) {
	//		hepb.push(0, outb);
	//	}

	//	uint64_t compa(UINT64_MAX);
	//	uint64_t compb(0);
	//	int dummya(0), dummyb(0);
	//	while (hepa.try_pop(dummya, compa)) {
	//		hepb.try_pop(dummyb, compb);
	//		Assert::IsTrue(compa == compb, L"Mismatch between pushed entries and popped entries");
	//	}
	//	

	//	Assert::IsTrue(list.size() == 0, L"Bad size");

	//}
};
}