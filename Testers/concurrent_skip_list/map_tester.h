#pragma once

#include <gdul/containers/concurrent_unordered_map.h>
#include <gdul/containers/concurrent_map.h>
#include "../Common/util.h"
#include <gdul/utility/delegate.h>
#include <vector>

namespace gdul {

class map_tester
{
public:

	void perform_tests(int iterations);
	void perform_speed_tests(int iterations);


private:


	void do_insertions(int inserts);
	void verify_insertions();

	void check_insertion_speed(int inserts);
	void check_lookup_speed(int inserts);


	concurrent_map<int, int> m_sl;
	concurrent_unordered_map<int, int> m_hm;

	static thread_local std::vector<int> t_keys;
};
}