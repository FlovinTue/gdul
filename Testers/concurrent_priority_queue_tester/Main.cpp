
#include "pch.h"
#include <iostream>

//#include <vld.h>
#include <gdul\WIP_concurrent_priority_queue\concurrent_priority_queue.h>
#include <thread>
#include <random>


int main()
{
	const uint32_t numOps(1000);
	const uint32_t numthreads(8);
	
	gdul::concurrent_priority_queue<uint64_t, int, numOps * numthreads> list;
	std::pair<uint64_t, int> in(5, 0);
	list.insert(in);
	std::pair<uint64_t, int> in2(4, 0);
	list.insert(in2);

	//gdul::concurrent_queue<uint64_t> inQue;
	//gdul::concurrent_queue<uint64_t> outQue;
	//
	//std::random_device rd;
	//std::mt19937 rng(rd());
	//
	//std::atomic<uint32_t> popCount(0);
	//
	//std::atomic<bool> begin(false);
	//std::atomic<uint32_t> finished(0);
	//
	//auto lam = [&list, &begin, &finished, numOps, &rng, &popCount, &inQue, &outQue]() {
	//	inQue.reserve(numOps);
	//	outQue.reserve(numOps);
	//
	//	while (!begin)
	//		std::this_thread::yield();
	//
	//	for (uint32_t i = 0; i < numOps; ++i) {
	//		std::pair<uint64_t, int> a(rng(), rng());
	//		std::pair<uint64_t, int> b(rng(), rng());
	//
	//		list.insert(a);
	//		list.insert(b);
	//
	//		std::pair<uint64_t, int> out;
	//		list.try_pop(out);
	//
	//		inQue.push(a.first);
	//		inQue.push(b.first);
	//		outQue.push(out.first);
	//	}
	//
	//	++finished;
	//};
	//
	//
	//for (uint32_t i = 0; i < numthreads; ++i) {
	//	std::thread thread(lam);
	//	thread.detach();
	//}
	//begin = true;
	//
	//while (finished.load() != numthreads) {
	//	std::this_thread::sleep_for(std::chrono::microseconds(10));
	//}
	//
	//assert(list.size() == numOps * numthreads && "Bad size");
	//
	//uint64_t last(0);
	//std::pair<uint64_t, int> out;
	//while (list.try_pop(out)) {
	//	assert(!(out.first < last) && "Popped value less than last");
	//	last = out.first;
	//	outQue.push(out.first);
	//}
	//
	//heap<int> hepa;
	//uint64_t outa(0);
	//while (outQue.try_pop(outa)) {
	//	hepa.push(0, outa);
	//}
	//
	//heap<int> hepb;
	//uint64_t outb(0);
	//while (inQue.try_pop(outb)) {
	//	hepb.push(0, outb);
	//}
	//
	//uint64_t compa(UINT64_MAX);
	//uint64_t compb(0);
	//int dummya(0), dummyb(0);
	//while (hepa.try_pop(dummya, compa)) {
	//	hepb.try_pop(dummyb, compb);
	//	assert(compa == compb && "Mismatch between pushed entries and popped entries");
	//}
	//
	//
	//assert(list.size() == 0 && "Bad size");
	//
	//
	//std::cout << "hello" << std::endl;
}

