#pragma once

#include <gdul/WIP/concurrent_hash_map_v3.h>
#include <gdul/containers/concurrent_skip_list.h>
#include "../Common/util.h"
#include <gdul/utility/delegate.h>
#include <vector>

namespace gdul {

class map_tester
{
public:

	void perform_tests(int iterations);



private:


	void do_insertions(int inserts);
	void verify_insertions();

	concurrent_skip_list<int, int> m_sl;
	concurrent_hash_map<int, int> m_hm;

	static thread_local std::vector<int> t_keys;
};
}