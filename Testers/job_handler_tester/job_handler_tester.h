#pragma once

#include <gdul\job_handler_master.h>

class job_handler_tester
{
public:
	job_handler_tester();
	~job_handler_tester();

	void init();

	float run_all_tests(std::size_t numInserts);

	float run_consumption_test(std::size_t numInserts);
	float run_construction_test(std::size_t numInserts);
	float run_mixed_test(std::size_t numInserts);


	gdul::job_handler myHandler;
};

