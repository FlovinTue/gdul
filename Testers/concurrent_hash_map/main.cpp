

#include <gdul/WIP/concurrent_hash_map.h>


int main()
{
	gdul::concurrent_hash_map<std::uint64_t, float> m;

	for (std::size_t i = 0; i < 10000; ++i) {
		m.insert({ i, (float)i });
	}

	for (std::size_t i = 0; i < 10000; ++i) {
		m.unsafe_erase(i);
	}

	return 0;
}