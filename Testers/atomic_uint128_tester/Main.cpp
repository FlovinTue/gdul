
#include "pch.h"
#include <iostream>
#include <gdul\atomic_uint128.h>


int main()
{
	using namespace gdul;

	atomic_uint128 one;
	atomic_uint128 two(uint128{});

	uint128 valone(1);
	two.store(valone);

	uint128 valtwo(2);
	uint128 exone(two.exchange(valtwo));

	uint128 valthree(3);
	valthree = two.load();

	uint128 valfour(4);
	two.compare_exchange_strong(valthree, valfour);

	uint128 valfive(0);
	valfive = two.fetch_add_to_u64(1, 0);

	uint128 valsix(0);
	valsix = two.fetch_sub_to_u32(1, 0);

	uint128 valseven(0);
	valseven = two.my_val();

	uint128 high(1, 1);
	uint128 copy(high);

	return 0;
}