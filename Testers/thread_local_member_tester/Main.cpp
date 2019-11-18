
#include "pch.h"
#include <iostream>
#include <gdul\WIP_thread_local_member\thread_local_member.h>
#include <thread>
#include <vld.h>
#include <gdul\atomic_shared_ptr\atomic_shared_ptr.h>
#include <gdul\concurrent_queue\concurrent_queue.h>
#include <functional>
#include <vector>
#include "tester.h"

int main()
{
	gdul::tester<> tester;
	tester.execute();

	return 0;
}