
#include "pch.h"
#include <iostream>
#include <gdul\atomic_128\atomic_128.h>
#include "../Common/Timer.h"
#include <atomic>

struct custom_struct
{
	float one, two;
	int three;
	bool four, five, six;
};

int main()
{
	using namespace gdul;

	atomic_u128 aliased(5);

	{
		atomic_128<u128> one;
		atomic_128<u128> two(u128{});

		u128 valone(1);
		two.store(valone);

		u128 valtwo(2);
		u128 exone(two.exchange(valtwo));

		u128 valthree(3);
		valthree = two.load();

		u128 valfour(4);
		two.compare_exchange_strong(valthree, valfour);

		u128 valfive(0);
		valfive = two.fetch_add_to_u64(1, 0);

		u128 valsix(0);
		valsix = two.fetch_sub_to_u32(1, 0);

		u128 valseven(0);
		valseven = two.my_val();

		two.store(2);
		u128 valeight(0);
		valeight = two.exchange_u8(5, 0);

		u128 high(1, 1);
		u128 copy(high);
	}
	{
		atomic_128<custom_struct> one;

		custom_struct valone;
		one.store(valone);
		custom_struct extwo(one.exchange(valone));

		custom_struct valthree = one.load();

		custom_struct valfour;
		one.compare_exchange_strong(valthree, valfour);

		custom_struct valfive;
		valfive = one.my_val();
	}

	timer<float> time;
	{
	u128 exp(0);
	atomic_u128 val(1);
	for (uint32_t i = 0; i < 1000000000; ++i)
	{
		u128 des(i);
		val.compare_exchange_strong(exp, des);
	}
	std::cout << "first time: " << time.get() << std::endl;
	}
	{
		time.reset();

		uint64_t exp(0);
		std::atomic<uint64_t> val(1);
		for (uint32_t i = 0; i < 1000000000; ++i)
		{
			uint64_t des(i);
			val.compare_exchange_strong(exp, des);
		}
		std::cout << "second time: " << time.get() << std::endl;
	}


	return 0;
}