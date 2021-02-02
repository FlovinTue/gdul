

#include <gdul/WIP/concurrent_hash_map_v2.h>

#pragma warning(disable:4530)
#include <iostream>

int main()
{
	gdul::concurrent_hash_map<std::uint64_t, float> m;

	m.insert(std::make_pair(1ull, 1.f));


	//for (std::size_t i = 0; i < 10000; ++i) {
	//	m.insert({ i, (float)i });
	//}

	//for (std::size_t i = 0; i < 10000; ++i) {
	//	m.unsafe_erase(i);
	//}

	//gdul::concurrent_hash_map<std::uint64_t, float> itrcheck;
	//std::pair<decltype(itrcheck)::iterator, bool> a(itrcheck.insert({ 1ull, 1.f }));
	//std::pair<decltype(itrcheck)::iterator, bool> b(itrcheck.insert({ 2ull, 2.f }));
	//std::pair<decltype(itrcheck)::iterator, bool> c(itrcheck.insert({ 3ull, 3.f }));

	//decltype(itrcheck)::iterator itr(a.first);
	//++itr;
	//++itr;
	//++itr;

	//decltype(itrcheck)::const_iterator citr(itrcheck.begin());
	//++citr;
	//++citr;
	//++citr;

	//decltype(itrcheck)::const_reverse_iterator ritr(itrcheck.rbegin());
	//++ritr;
	//++ritr;
	//++ritr;

	//const bool endcheck(citr == itrcheck.end());
	//const bool rendcheck(ritr == itrcheck.rend());

	return 0;
}