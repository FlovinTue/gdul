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
#pragma warning(push)
#pragma warning(disable : 4324)

#include <thread>
#include <array>
#include <vector>
#include <chrono>
#include <concurrent_vector.h>

#include <gdul\WIP_job_handler\job.h>
#include <gdul\concurrent_object_pool\concurrent_object_pool.h>
#include <gdul\WIP_job_handler\job_handler_commons.h>
#include <gdul\WIP_job_handler\job_impl.h>
#include <gdul\WIP_job_handler\job_impl_allocator.h>

namespace gdul {

namespace job_handler_detail {

class job_impl;
}
struct job_handler_info
{
	std::uint16_t myMaxWorkers = static_cast<std::uint16_t>(std::thread::hardware_concurrency());

	// Thread priority as defined in WinBase.h
	std::uint32_t myWorkerPriorities = 0;

	// Number of milliseconds passed before an unemployed worker starts
	// sleeping away time instead of yielding
	std::uint16_t mySleepThreshhold = 250;

	//// This will be run per worker upon launch. Good to have in case there is 
	//// thread specific initializations that needs to be done.
	//job myOnThreadLaunch = job([]() {});

	//// This will be run per worker upon exit. Good to have in case there is 
	//// thread specific clean up that needs to be done.
	//job myOnThreadExit = job([]() {});
};

class job_handler
{
public:
	using allocator_type = job_handler_detail::allocator_type;
	using job_impl_shared_ptr = job_handler_detail::job_impl::job_impl_shared_ptr;

	static thread_local job this_job;

	job_handler();
	job_handler(allocator_type& allocator);
	~job_handler();

	void Init(const job_handler_info& info = job_handler_info());
 	void reset();

	// Callable will allocate if size is above job_handler_detail::Callable_Max_Size_No_Heap_Alloc
	// It needs to have operator() defined with signature void(void). 
	// Priority corresponds to the internal queue it will be placed in. Priority 0 -> highest. 
	// job_handler_detail::Priority_Granularity defines the number of queueus
	template <class Callable>
	job make_job(Callable&& callable, std::uint8_t priority);

	// Callable will allocate if size is above job_handler_detail::Callable_Max_Size_No_Heap_Alloc
	// It needs to have operator() defined with signature void(void). 
	template <class Callable>
	job make_job(Callable&& callable);

private:
	friend class job_handler_detail::job_impl;
	friend class job;

	template <class Callable>
	job_impl_shared_ptr make_job_impl(Callable&& callable, std::uint8_t priority);


	void enqueue_job(job_impl_shared_ptr job);

	void launch_worker(std::uint32_t workerIndex);

	void work();
	void idle();

	job_impl_shared_ptr fetch_job();

	// The distribution of queue consumption is meant to happen in a log2 fashion.
	// The next queue down gets consumed from half as much...
	std::uint8_t generate_priority_index();

	static thread_local std::chrono::high_resolution_clock ourSleepTimer;
	static thread_local std::chrono::high_resolution_clock::time_point ourLastJobTimepoint;

	static thread_local std::size_t ourLastJobSequence;
	static thread_local std::size_t ourPriorityDistributionIteration;

	allocator_type myMainAllocator;

	concurrent_object_pool<job_handler_detail::job_impl_chunk_rep, job_handler_detail::allocator_type> myJobImplChunkPool;

	concurrent_queue<job_impl_shared_ptr, allocator_type> myJobQueues[job_handler_detail::Priority_Granularity];
	
	job_handler_detail::job_impl_allocator<uint8_t> myJobImplAllocator;

	std::array<std::thread, 32> myWorkers;

	job_handler_info myInitInfo;

	std::atomic<bool> myIsRunning;
};

template<class Callable>
inline job job_handler::make_job(Callable && callable, std::uint8_t priority)
{
	return job(make_job_impl(std::forward<Callable&&>(callable), priority));
}

template<class Callable>
inline job job_handler::make_job(Callable && callable)
{
	return make_job(std::forward<Callable&&>(callable), job_handler_detail::Default_Job_Priority);
}

template<class Callable>
inline job_handler::job_impl_shared_ptr job_handler::make_job_impl(Callable&& callable, std::uint8_t priority)
{
	assert(priority < job_handler_detail::Priority_Granularity && "Priority value out of bounds");

	const uint8_t _priority(std::clamp<std::uint8_t>(priority, 0, job_handler_detail::Priority_Granularity - 1));

	return make_shared<job_handler_detail::job_impl, job_handler_detail::job_impl_allocator<std::uint8_t>>
		(
			myJobImplAllocator, 
			this, 
			std::forward<Callable&&>(callable), 
			_priority, 
			myMainAllocator
			);
}

}
#pragma warning(pop)

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