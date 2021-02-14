#include "map_tester.h"
#include <iostream>
#include "../Common/thread_pool.h"

namespace gdul {

shared_ptr<thread_pool> g_workers;

thread_local std::vector<int> map_tester::t_keys;

void map_tester::perform_tests(int iterations)
{
	g_workers = make_shared<thread_pool>();

	for (int iter = 0; iter < iterations; ++iter) {
		int inserts(400);

		std::function<void()> work([this, inserts]() {do_insertions(inserts); verify_insertions(); t_keys.clear(); });

		for (int i = 0; i < g_workers->worker_count(); ++i) {
			g_workers->add_task(work);
		}

		while (g_workers->has_unfinished_tasks()) {
			std::this_thread::sleep_for(std::chrono::microseconds(1));
		}

		m_sl.unsafe_clear();
		m_hm.unsafe_clear();
	}

	std::cout << "Performed " << iterations << " iterations" << std::endl;
}

void map_tester::do_insertions(int inserts)
{
	for (std::size_t i = 0; i < inserts; ++i) {
		t_keys.push_back((int)csl_detail::t_rng());
		m_hm.insert(std::make_pair(t_keys.back(), t_keys.back()));
		m_sl.insert(std::make_pair(t_keys.back(), t_keys.back()));
	}
}

void map_tester::verify_insertions()
{
	for (auto& elem : t_keys) {
		GDUL_ASSERT(m_sl.find(elem) != m_sl.end());
		GDUL_ASSERT(m_hm.find(elem) != m_hm.end());
	}
}
}