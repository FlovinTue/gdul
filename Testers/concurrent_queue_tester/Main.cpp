// ThreadSafeConsumptionQueue.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <gdul/concurrent_queue/concurrent_queue.h>

int main()
{
	gdul::concurrent_queue<int> q;
	q.push(1);
	q.push(2);
	q.push(3);


	int out1(0);
	int out2(0);
	int out3(0);

	const bool res1(q.try_pop(out1));
	const bool res2(q.try_pop(out2));
	const bool res3(q.try_pop(out3));

	return 0;
}

