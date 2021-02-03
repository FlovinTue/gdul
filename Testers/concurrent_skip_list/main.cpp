


#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING

#include "../Common/thread_pool.h"
#include "../Common/Timer.h"
#include "../Common/util.h"
#include <gdul/containers/concurrent_skip_list.h>
#include <gdul/memory/atomic_shared_ptr.h>
#include <gdul/WIP/concurrent_hash_map.h>
#include <iostream>
#include <concurrent_unordered_map.h>
#include <unordered_set>
#include <mutex>

constexpr uint32_t inserts = 256 * 8;
constexpr size_t iterations(100000);

struct container
{
	gdul::thread_pool tp = gdul::thread_pool(std::numeric_limits<std::uint8_t>::max());
	gdul::concurrent_skip_list<uint32_t, uint32_t, inserts> sl;
	std::vector<uint32_t> keys;
	gdul::concurrent_hash_map<uint32_t, uint32_t> hm = gdul::concurrent_hash_map<uint32_t, uint32_t>(inserts);
	concurrency::concurrent_unordered_map<uint32_t, uint32_t> us;
	//std::unordered_map<int, int> us;
};

gdul::shared_ptr<container> c;

std::mutex mtx;

thread_local size_t nonsense = 0;
size_t global = 0;
void find()
{
	const decltype(c->sl)& sl(c->sl);
	sl;

	for (auto& itr : c->keys) {
		//auto result(c->us.find(itr));
		//auto result = sl.find(itr);
		auto result(c->hm.find(itr));

		nonsense += (*result).second;

	}
	global += nonsense;
}
void insert(int ix)
{
	mtx.lock();
	std::vector<uint32_t> keys;
	for (uint32_t i = 0; i < inserts / std::thread::hardware_concurrency(); ++i) {
		const uint32_t k(gdul::csl_detail::t_rng() + ix);
		keys.push_back(k);
		c->sl.insert({ k, k });
		c->us.insert({ k, k });
		c->hm.insert({ k,k });
	}

	for (auto k : keys) {
		c->keys.push_back(k);
	}
	mtx.unlock();
}


int main()
{
	gdul::concurrent_skip_list<int, int> itrTest;
	itrTest.insert({ 1,1 });
	auto itrA = itrTest.begin();
	auto itrB = itrTest.begin();
	itrA++;
	++itrB;

	c = gdul::make_shared<container>();

	for (int i = 0; i < 8; ++i) {
		c->tp.add_task([i]() {insert(i); });
	}

	while (c->tp.has_unfinished_tasks()) {
		std::this_thread::yield();
	}

	for (int i = 0; i < c->keys.size(); ++i) {
		GDUL_ASSERT(c->sl.find(c->keys[i]) != c->sl.end() && "Failed at");
		GDUL_ASSERT(c->hm.find(c->keys[i]) != c->hm.end() && "Failed at");
	}

	gdul::timer<float> t;
	for (std::size_t i = 0; i < iterations; ++i) {
		c->tp.add_task([]() { find(); });
	}

	while (c->tp.has_unfinished_tasks()) {
		std::this_thread::yield();
	}

	decltype(itrTest)::iterator itr;
	decltype(itrTest)::const_iterator citr;

	itr.operator==(citr);
	itr.operator!=(citr);

	std::cout << iterations * inserts << " finds took: " << t.get() << " seconds" << std::endl;
}
