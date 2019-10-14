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
#include <gdul\WIP_job_handler\job.h>
#include <gdul\concurrent_object_pool\concurrent_object_pool.h>
#include <concurrent_vector.h>

#ifndef THREAD_PRIORITY_NORMAL
#define THREAD_PRIORITY_NORMAL 0
#endif

namespace gdul {

namespace job_handler_detail {

constexpr std::uint8_t Num_Priority_Queues = 4;
using allocator_type = std::allocator<std::uint8_t>;

}
struct job_handler_info
{
	uint16_t myNumWorkers = static_cast<std::uint16_t>(std::thread::hardware_concurrency() * 2);

	// The maximum number of workers that are allowed to work on asynchronous 
	// jobs concurrently. Should be tuned so that synchronous operation is 
	// not interrupted
	uint16_t myMaxAsyncWorkers = static_cast<std::uint16_t>(ceil(static_cast<float>(myNumWorkers) / 3.0f));

	// Thread priority as defined in WinBase.h
	uint32_t myWorkerPriorities = THREAD_PRIORITY_NORMAL;

	// Number of milliseconds passed before an unemployed worker starts
	// sleeping away time instead of yielding
	uint16_t mySleepThreshhold = 250;

	// This will be run per worker upon launch. Good to have in case there is 
	// thread specific initializations that needs to be done.
	job myOnThreadLaunch = job([]() {});

	// This will be run per worker upon exit. Good to have in case there is 
	// thread specific clean up that needs to be done.
	job myOnThreadExit = job([]() {});
};

class job_handler
{
public:
	job_handler();
	~job_handler();

	static constexpr size_t Job_Queues_Init_Alloc = 16;

	void Init(const job_handler_info& info = job_handler_info());

	// Restores initial values & recycles all job sequences in use


	void submit(const job& job);
	void submit(job&& job);

	void reset();

	void run();

	void abort();

private:


private:
	void launch_worker(std::uint32_t workerIndex);

	void work();
	void idle();

	job fetch_job();

	void set_thread_name(const std::string& name);

	static thread_local std::chrono::high_resolution_clock ourSleepTimer;
	static thread_local std::chrono::high_resolution_clock::time_point ourLastJobTimepoint;

	static thread_local size_t ourLastJobSequence;

	const job myIdleJob;

	concurrent_queue<job, job_handler_detail::allocator_type> myJobQueues[job_handler_detail::Num_Priority_Queues];

	std::vector<std::thread> myWorkers;

	job_handler_info myInitInfo;

	std::atomic<bool> myIsRunning;
};

}


// Usage ideas / functionality
// Asynchronous submission of delegates with priorities
// Synchronous submission of delegates with posibility of parallel execution

// Submission of delegates from within current job, suspending current job until completed? ? 
// -- would be practical. Run on current worker, farming out 'some' jobs to other workers?

// Use some form of fiber system for execution? (Allowing suspension in the middle of a job, 
// only to be resumed by the first avaliable thread)

// Change design to each thread having a native sequence? Hmm. May harm modularity and simplicity..

// Use externally supplied threads or internal ones?

// Optimize for throughput or latency? Or both?

// .. Array scatter-gather helper?
// Needs Source array.. oor.. begin -> end iterator. Probably better.
// Needs operation to perform... Hmm generalize more.. hmmm
// And an output array? ( or output begin->end iterator) 
// Use cases? Hmm. Need to generalize for more use cases?
// For example sort? How would that work?
// Culling? Output arrays would not match input.
// 
// Fundamental rebuild:
// A choice of number of queues, each next one with a lower priority
// Support multiple dependancies
// Job dependancies structured in a graph
// Jobs submitted to their queues by post job examination of children
// Consumption from queues with a log2 relationship to lower priority?