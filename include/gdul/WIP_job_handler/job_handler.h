// Copyright(c) 2019 Flovin Michaelsen
// 
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
// 
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
// 
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#include <vector>
#include <thread>
#include <chrono>
#include <gdul\WIP_job_handler\job_sequence.h>
#include <gdul\WIP_job_handler\job.h>
#include <gdul\concurrent_object_pool\concurrent_object_pool.h>
#include <concurrent_vector.h>

#ifndef THREAD_PRIORITY_NORMAL
#define THREAD_PRIORITY_NORMAL 0
#endif

namespace gdul {

struct job_handler_info
{
	uint16_t myNumWorkers = static_cast<uint16_t>(std::thread::hardware_concurrency() * 2);

	// The maximum number of workers that are allowed to work on asynchronous 
	// jobs concurrently. Should be tuned so that synchronous operation is 
	// not interrupted
	uint16_t myMaxAsyncWorkers = static_cast<uint16_t>(ceil(static_cast<float>(myNumWorkers) / 3.0f));

	// Thread priority as defined in WinBase.h
	uint32_t myWorkerPriorities = THREAD_PRIORITY_NORMAL;

	// Number of milliseconds passed before an unemployed worker starts
	// sleeping away time instead of yielding
	uint16_t mySleepThreshhold = 250;

	// This will be run per worker upon launch. Good to have in case there is 
	// thread specific initializations that needs to be done.
	std::function<void()> myOnLaunchJob = []() {};

	// This will be run per worker upon exit. Good to have in case there is 
	// thread specific clean up that needs to be done.
	std::function<void()> myOnExitJob = []() {};
};
class job_handler
{
public:
	job_handler();
	~job_handler();

	static constexpr size_t Job_Queues_Init_Alloc = 16;

	void Init(const job_handler_info& info = job_handler_info());

	// Restores initial values & recycles all job sequences in use
	void reset();

	void run();

	void abort();

	static thread_local job_sequence_impl* this_JobSequence;
private:
	friend class job_sequence;

	job_sequence_impl* create_job_sequence();

private:
	void launch_worker(uint32_t workerIndex);

	void work();
	void idle();

	job fetch_job();

	void set_thread_name(const std::string& name);

	static thread_local std::chrono::high_resolution_clock ourSleepTimer;
	static thread_local std::chrono::high_resolution_clock::time_point ourLastJobTimepoint;

	static thread_local size_t ourLastJobSequence;

	const job myIdleJob;

	concurrent_object_pool<job_sequence_impl> myJobSequencePool;

	concurrency::concurrent_vector<job_sequence_impl*> myJobSequences;

	std::vector<std::thread> myWorkers;

	job_handler_info myInitInfo;

	std::atomic<bool> myIsRunning;
};

}