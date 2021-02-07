

#include <gdul/containers/concurrent_unordered_map.h>
#include <gdul/utility/packed_ptr.h>

#pragma warning(disable:4530)
#include <iostream>


struct mystruct
{
	int mem = 10;
	uint32_t other = 533;
};

enum myenum : uint32_t
{
	donkey,
	cow,
	sheep,
	owl,
	horse,
	house,
	cat,
	dog,
	flea,
	duck,
};

int main()
{
	gdul::concurrent_unordered_map<std::uint64_t, float> m;

	auto result(m.insert(std::make_pair(1ull, 1.f)));
	auto result2(m.insert(std::make_pair(1ull, 1.f)));

	auto find(m.find(1));

	for (std::size_t i = 0; i < 10000; ++i) {
		m.insert({ i, (float)i });
	}

	for (std::size_t i = 0; i < 10000; ++i) {
		m.unsafe_erase(i);
	}

	gdul::concurrent_unordered_map<std::uint64_t, float> itrcheck;
	std::pair<decltype(itrcheck)::iterator, bool> a(itrcheck.insert({ 1ull, 1.f }));
	std::pair<decltype(itrcheck)::iterator, bool> b(itrcheck.insert({ 2ull, 2.f }));
	std::pair<decltype(itrcheck)::iterator, bool> c(itrcheck.insert({ 3ull, 3.f }));

	decltype(itrcheck)::iterator itr(a.first);
	++itr;
	++itr;
	++itr;

	decltype(itrcheck)::const_iterator citr(itrcheck.begin());
	++citr;
	++citr;
	++citr;

	decltype(itrcheck)::const_reverse_iterator ritr(itrcheck.rbegin());
	++ritr;
	++ritr;
	++ritr;

	const bool endcheck(citr == itrcheck.end());
	const bool rendcheck(ritr == itrcheck.rend());

	gdul::packed_ptr<mystruct, myenum> pp(new mystruct(), duck);

	delete pp.ptr();


	gdul::concurrent_unordered_map<int, int> capacityTest(11);

	const std::size_t capA(capacityTest.capacity());

	capacityTest.reserve(12);

	const std::size_t capB(capacityTest.capacity());

	return 0;
}