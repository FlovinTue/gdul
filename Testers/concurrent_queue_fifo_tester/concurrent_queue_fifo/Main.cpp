


#include "pch.h"
#include <vld.h>

#include <gdul/WIP_concurrent_queue_fifo/concurrent_queue_fifo.h>

int main()
{
	gdul::concurrent_queue_fifo<int> que;

	for (uint32_t i = 0; i < 128; ++i)
	{
		que.push(i + 1);
	}

	for (uint32_t i = 0; i < 128; ++i)
	{
		int out;
		bool result = que.try_pop(out);
		assert(result && "Should have succeeded");
		assert(out == i + 1 && "Bad out data");
	}

	return 0;
}