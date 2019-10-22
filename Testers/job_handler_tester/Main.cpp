// JobSystem.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <gdul\job_handler_master.h>
#include <iostream>

int main()
{
	std::allocator<uint8_t> alloc;

	gdul::job_handler handler(alloc);

	gdul::worker worker(handler.create_worker());
	worker.set_core_affinity(0);
	worker.set_execution_priority(4);
	worker.set_queue_affinity(1);
	worker.set_name("Hard second queue worker");
	//worker.enable();
	
	bool retired(worker.is_retired());
	retired;

	gdul::job hey(handler.make_job([]() {std::cout << "Hey" << std::endl; }, 0));

	gdul::job there(handler.make_job([]() {std::cout << "there" << std::endl; }, 0));
	there.add_dependency(hey);

	gdul::job mr(handler.make_job([]() {std::cout << "mr" << std::endl; }, 0));
	mr.add_dependency(there);

	gdul::job guy(handler.make_job([]() {std::cout << "guy!" << std::endl; }, 0));

	for (uint16_t i = 0; i < 4096; ++i) {
		uint8_t prio(i % gdul::job_handler_detail::Priority_Granularity);

		gdul::job nice(handler.make_job([prio]() {std::cout << "nice (" << uint32_t(prio) << " prio)" << std::endl; }, prio));
		nice.add_dependency(mr);
		guy.add_dependency(nice);
		nice.enable();
	}

	mr.enable();
	hey.enable();
	guy.enable();
	there.enable();

	guy.wait_for_finish();

	std::cout << "Hello World!\n";
}