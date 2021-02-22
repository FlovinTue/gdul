#include "map_tester.h"
#include <iostream>
#include "../Common/thread_pool.h"
#include "../Common/Timer.h"

namespace gdul {

shared_ptr<thread_pool> g_workers;

thread_local std::vector<int> map_tester::t_keys;

void map_tester::perform_tests(int iterations)
{
	g_workers = make_shared<thread_pool>();

		int inserts(400);
	for (int iter = 0; iter < iterations; ++iter) {

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

void map_tester::perform_speed_tests(int iterations)
{
	int inserts(400);
	for (int i = 0; i < iterations; ++i) {
		check_insertion_speed(inserts);
		check_lookup_speed(inserts);
	}
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
void map_tester::check_insertion_speed(int inserts)
{
	std::cout << "Testing insertion speed using " << inserts << " inserts" << std::endl;
	
	constexpr int passes(10000);

	gdul::timer<float> time;
	for (int i = 0; i < passes; ++i) {
		for (int j = 0; j < inserts; ++j) {
			m_hm.insert({ csl_detail::t_rng(), 0 });
		}
		m_hm.unsafe_clear();
	}

	const float hmTime(time.get());

	time.reset();

	for (int i = 0; i < passes; ++i) {
		for (int j = 0; j < inserts; ++j) {
			m_sl.insert({ csl_detail::t_rng(), 0 });
		}
		m_sl.unsafe_clear();
	}

	const float slTime(time.get());
	
	std::cout << "Finished insert speed test.\n";
	std::cout << "Hash map: " << (hmTime * 1000000.f) / (inserts * passes) << " microseconds per insertion" << std::endl;
	std::cout << "Skip list: " << (slTime * 1000000.f) / (inserts * passes) << " microseconds per insertion" << std::endl;
}
void map_tester::check_lookup_speed(int inserts)
{
	std::cout << "Testing lookup speed using " << inserts << " inserts" << std::endl;

	do_insertions(inserts);

	constexpr int passes(10000);

	long int nonsense(0);

	gdul::timer<float> time;
	for (int i = 0; i < passes; ++i) {
		for (auto& k : t_keys) {
			nonsense += m_hm.find(k)->second;
		}
	}

	const float hmTime(time.get());

	time.reset();

	for (int i = 0; i < passes; ++i) {
		for (auto& k : t_keys) {
			nonsense += m_sl.find(k)->second;
		}
	}

	const float slTime(time.get());

	t_keys.clear();
	m_hm.unsafe_clear();
	m_sl.unsafe_clear();

	std::cout << "Finished lookup speed test.\n";
	std::cout << "Hash map: " << (hmTime * 1000000.f) / (inserts * passes) << " microseconds per lookup" << std::endl;
	std::cout << "Skip list: " << (slTime * 1000000.f) / (inserts * passes) << " microseconds per lookup" << std::endl;
}
}