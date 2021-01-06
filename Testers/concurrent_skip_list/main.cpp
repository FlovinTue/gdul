


#define _SILENCE_CXX17_OLD_ALLOCATOR_MEMBERS_DEPRECATION_WARNING

#include "../Common/thread_pool.h"
#include "../Common/Timer.h"
#include "../Common/util.h"
#include <gdul/concurrent_skip_list/concurrent_skip_list.h>
#include <gdul/atomic_shared_ptr/atomic_shared_ptr.h>
#include <iostream>
#include <concurrent_unordered_set.h>
#include <unordered_set>
#include <mutex>

constexpr uint32_t inserts = 256 * 8;

struct container
{
	gdul::thread_pool tp = gdul::thread_pool(std::numeric_limits<std::uint8_t>::max());
	gdul::concurrent_skip_list<uint32_t, uint32_t, inserts> sl;
	std::vector<uint32_t> keys;
};

gdul::shared_ptr<container> c;
concurrency::concurrent_unordered_set<int> us;

std::mutex mtx;
//std::unordered_set<int> us;


void find()
{
	const decltype(c->sl)& sl(c->sl);
	sl;

	for (std::size_t i = 0; i < c->keys.size();++i) {
		//auto itr(us.find(c->keys[i]));
		sl.find(c->keys[i]);
	}
}
void insert()
{
	std::vector<uint32_t> keys;
	for (uint32_t i = 0; i < inserts / std::thread::hardware_concurrency(); ++i) {
		const uint32_t k(gdul::csl_detail::t_rng());
		keys.push_back(k);
		c->sl.insert({ k, k });
		us.insert(k);
	}

	mtx.lock();
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
		c->tp.add_task([]() {insert(); });
	}

	while (c->tp.has_unfinished_tasks()) {
		std::this_thread::yield();
	}

	for (int i = 0; i < c->keys.size(); ++i) {
		GDUL_ASSERT(c->sl.find(c->keys[i]) != c->sl.end() && "Failed at");
	}

	gdul::timer<float> t;
	for (std::size_t i = 0; i < 10000; ++i) {
		c->tp.add_task([]() { find(); });
	}

	while (c->tp.has_unfinished_tasks()) {
		std::this_thread::yield();
	}

	decltype(itrTest)::iterator itr;
	decltype(itrTest)::const_iterator citr;

	itr.operator==(citr);
	itr.operator!=(citr);

	std::cout << 10000 * inserts << " finds took: " << t.get() << " seconds" << std::endl;
}
