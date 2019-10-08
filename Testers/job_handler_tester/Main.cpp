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
	while (spinFlag.test_and_set()) std::this_thread::yield();

	//mtx.lock();
	std::cout << "Parallel# " << aIndex << std::endl;
	//mtx.unlock();
	spinFlag.clear();
}
void Sequencial(uint32_t aIndex)
{
	while (spinFlag.test_and_set()) std::this_thread::yield();
	assert(++count == aIndex && "index mismatch");
	//mtx.lock();
	std::cout << "Sequencial# " << aIndex << std::endl;
	//mtx.unlock();
	spinFlag.clear();
}

int main()
{
	using namespace gdul;

	job_handler handler;

	job_handler_info initInfo;
	initInfo.myNumWorkers = 7;

	handler.Init(initInfo);

	job_sequence jobSequence(&handler);

	std::vector<int> heja;
	heja.push_back(5);
	heja.push_back(5);
	heja.push_back(5);
	heja.push_back(5);

	for (uint32_t j = 0; j < 200; ++j) {
		jobSequence.push([j]()
		{
			Sequencial(1 + j * 4);
		}, Job_layer::next);

		for (uint32_t i = 0; i < 5; ++i) {
			jobSequence.push([j]()
			{
				Parallel(1 + j * 4);
			}, Job_layer::back);
		}
		jobSequence.push([j]()
		{
			Sequencial(2 + j * 4);
		}, Job_layer::next);
		jobSequence.push([j]()
		{
			Sequencial(3 + j * 4);
		}, Job_layer::next);
		jobSequence.push([j]()
		{
			Sequencial(4 + j * 4);
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
}