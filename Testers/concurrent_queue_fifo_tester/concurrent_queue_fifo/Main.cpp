


#include "pch.h"
#include <vld.h>

#include <gdul/WIP_concurrent_queue_fifo/concurrent_queue_fifo_2.h>

int main()
{
	gdul::concurrent_queue_fifo<int> que;

	que.reserve(8);
	que.reserve(14);

	for (uint32_t i = 0; i < 9; ++i)
	{
		que.push(i + 1);
	}

	for (uint32_t i = 0; i < 9; ++i)
	{
		int out;
		que.try_pop(out);
	}

	return 0;
}
