


#include "pch.h"
#include <iostream>
#include <atomic>
#include <gdul/WIP_concurrent_queue_fifo/concurrent_queue_fifo.h>

int main()
{
	gdul::concurrent_queue_fifo<int> que(16);
	que.push(1);
	int out;
	que.try_pop(out);

	return 0;
}
