// JobSystem.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include <gdul\WIP_job_handler\job_sequence.h>
#include <gdul\WIP_job_handler\job_handler.h>
#include <mutex>

std::atomic_flag spinFlag;
std::mutex mtx;

std::atomic<uint32_t> count = 0;

void Parallel(uint32_t aIndex)
{
	aIndex;
	//if (count.load() != aIndex) {
	//	bool test = true; test;
	//}
	//while (spinFlag.test_and_set()) std::this_thread::yield();

	//mtx.lock();
	//std::cout << "Parallel# " << aIndex << std::endl;
	//mtx.unlock();
	//spinFlag.clear();
}
void Sequential(uint32_t aIndex)
{
	//while (spinFlag.test_and_set()) std::this_thread::yield();
	if (++count != aIndex) {
		bool test = true; test;
	}
	//mtx.lock();
	//std::cout << "Sequential# " << aIndex << std::endl;
	//mtx.unlock();
	//spinFlag.clear();
	
	if (!(aIndex % 10000)) {
		std::cout << "." << std::endl;
	}
}

int main()
{
	using namespace gdul;

	job_handler handler;

	job_handler_info initInfo;
	initInfo.myNumWorkers = 7;

	handler.Init(initInfo);

	job_sequence jobSequence(&handler);

	for (uint32_t j = 0; j < 1000000; ++j) {
		jobSequence.push([j]()
		{
			Sequential(1 + j * 4);
		}, Job_layer::next);

		for (uint32_t i = 0; i < 5; ++i) {
			jobSequence.push([j]()
			{
				Parallel(1 + j * 4);
			}, Job_layer::back);
		}
		jobSequence.push([j]()
		{
			Sequential(2 + j * 4);
		}, Job_layer::next);
		jobSequence.push([j]()
		{
			Sequential(3 + j * 4);
		}, Job_layer::next);
		jobSequence.push([j]()
		{
			Sequential(4 + j * 4);
		}, Job_layer::next);

		for (uint32_t i = 0; i < 5; ++i) {
			jobSequence.push([j]()
			{
				Parallel(4 + j * 4);
			}, Job_layer::back);
		}
		
	}
	jobSequence.push([&handler]() { handler.abort(); }, Job_layer::next);

	handler.run();

	while (spinFlag.test_and_set()) std::this_thread::yield();

	std::cout << "Hello World!\n";

	std::size_t blah;
}