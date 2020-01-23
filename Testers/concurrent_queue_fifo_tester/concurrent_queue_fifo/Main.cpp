


#include "pch.h"
#include <vld.h>

#include <gdul/WIP_concurrent_queue_fifo/concurrent_queue_fifo_2.h>

int main()
{
	gdul::concurrent_queue_fifo<int> que;
	que.push(1);

	int out(0);
	que.try_pop(out);
	return 0;
}
