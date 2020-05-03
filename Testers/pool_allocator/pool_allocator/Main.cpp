


#include "pch.h"
#include <iostream>
#include <tester.h>
#include <vld.h>



int main()
{
	//gdul::memory_pool pool;
	//pool.init<sizeof(gdul::test_struct), alignof(gdul::test_struct)>(2);
	//gdul::pool_allocator<gdul::test_struct> alloc(pool.create_allocator<gdul::test_struct>());
	//auto a = alloc.allocate();
	//auto b = alloc.allocate();
	//auto c = alloc.allocate();

	//alloc.deallocate(a);
	//alloc.deallocate(b);
	//alloc.deallocate(c);

	//for (auto i = 0; i < 128; ++i) {
	//	alloc.allocate();
	//}

	gdul::tester tester;
	tester.init();
	
	for (auto i = 0; i < 1000; ++i) {
		const float allocation = tester.test_allocation(2048);


		std::cout << "Result allocation: " << allocation << std::endl;
	}
	

	return 0;
}
