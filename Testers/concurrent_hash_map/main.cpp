

#include <gdul/concurrent_hash_map/concurrent_hash_map.h>


int main()
{
	gdul::concurrent_hash_map<std::uint64_t, float> m;

	decltype(m)::iterator itr(m.insert(std::make_pair(1284588ull, 1.f)).first);
	decltype(m)::iterator itr2(m.find(1284588ull));

	const decltype(m)& cm(m);

	decltype(m)::const_iterator citr2(cm.find(1284588ull));
	decltype(m)::const_iterator citr3(m.find(1284588ull));

	return 0;
}